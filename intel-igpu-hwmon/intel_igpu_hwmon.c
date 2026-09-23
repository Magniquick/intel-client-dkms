// SPDX-License-Identifier: GPL-2.0-only
/*
 * Expose power, temperature and memory clock of an Intel integrated GPU as an
 * hwmon device under the GPU, so tools that read <gpu>/hwmon/hwmonN/ (nvtop,
 * btop) see them. xe only registers its own hwmon on discrete parts. The
 * hwmon is named "xe" because btop's xe backend only accepts that name.
 *
 * energy1: RAPL PP1 (graphics) energy counter, MSR 0x641.
 * temp2:   GCD_MAX (graphics die max temp) from the Punit PMT telemetry.
 *          temp2 because nvtop reads temp2_input on xe.
 * mem_clock_mhz: current DRAM data rate (MT/s) of the active SAGV/QGV point.
 *          The telemetry gives the memory controller clock (QCLK), which is
 *          rate/4 at gear 2 and rate/2 at gear 1; it is matched against the
 *          QGV table the display engine publishes in MMIO. Not a standard
 *          hwmon attribute; hwmon has no frequency type.
 *
 * Nothing is written. The module reads MSRs and PMT telemetry SRAM on demand,
 * and the QGV table from GPU MMIO once at load.
 */
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/hwmon.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <asm/msr.h>

#define MSR_UNIT	0x606
#define MSR_PP1		0x641
#define MSR_TEMP_TARGET	0x1a2

/*
 * Arrow Lake-H Punit telemetry. Intel publishes no decode XML for this GUID;
 * the layout matches MTL's 0x130670b2 (Intel-PMT xml/MTL/0), where sample
 * group 20 holds GCD_MIN, GCD_MAX, ADM_MIN, ADM_MAX as S8 degrees C. Checked
 * under GPU load, where GCD_MAX follows RAPL PP1 power.
 */
#define PMT_GUID	0x1306a0b3
#define PMT_DEV		PCI_DEVFN(0x0a, 0)
#define PMT_GCD_QWORD	20
#define PMT_GCD_MAX_BYTE 1
/* Group 11 bits 55:48, QCLK_FREQ in units of 33.33 MHz (8533 MT/s -> 64) */
#define PMT_QCLK_QWORD	11
#define PMT_QCLK_SHIFT	48

/* Display ver 14+ QGV table, as read by intel_bw.c mtl_read_qgv_point_info() */
#define MTL_MEM_SS_INFO_GLOBAL	0x45700
#define MTL_QGV_POINT_LOW(p)	(0x10 + (p) * 8)	/* relative to GLOBAL */
#define MAX_QGV			16

/* Not in the installed headers (drivers/platform/x86/intel/pmt/telemetry.h) */
struct telem_endpoint;
struct telem_endpoint *
pmt_telem_find_and_register_endpoint(struct device *dev, u32 guid, u16 pos);
void pmt_telem_unregister_endpoint(struct telem_endpoint *ep);
int pmt_telem_read(struct telem_endpoint *ep, u32 id, u64 *data, u32 count);

static struct pci_dev *gpu, *pmt_pdev;
static struct telem_endpoint *pmt_ep;
static struct device *hwmon;
static DEFINE_MUTEX(lock);
static u32 last_raw;
static u64 acc_raw;
static unsigned int esu;	/* energy unit = 1/2^esu J */
static long tjmax_mc;
static unsigned int qgv_rate[MAX_QGV];	/* MT/s */
static unsigned int n_qgv;

/*
 * Fold the 32-bit counter into acc_raw on read only, as xe_hwmon_energy_get()
 * does: no timer. It wraps after ~2^32 * 61uJ = 262 kJ (~2.4 h at 30 W); a
 * reader idle for longer than that loses whole wraps and undercounts.
 */
static int update(void)
{
	u64 v;
	u32 raw;

	if (rdmsrq_safe(MSR_PP1, &v))
		return -EIO;
	raw = (u32)v;
	acc_raw += (u32)(raw - last_raw);
	last_raw = raw;
	return 0;
}

static umode_t is_visible(const void *d, enum hwmon_sensor_types type,
			  u32 attr, int ch)
{
	switch (type) {
	case hwmon_energy:
		return 0444;
	case hwmon_temp:
		if (!pmt_ep || ch != 1)
			return 0;
		if (attr == hwmon_temp_crit && !tjmax_mc)
			return 0;
		return 0444;
	default:
		return 0;
	}
}

static int read_gcd_max(long *val)
{
	u64 q;
	int ret;

	ret = pmt_telem_read(pmt_ep, PMT_GCD_QWORD, &q, 1);
	if (ret)
		return ret;
	*val = (long)(s8)(q >> (8 * PMT_GCD_MAX_BYTE)) * 1000;
	return 0;
}

/* QGV entry within 2% of @rate, or 0 */
static unsigned int qgv_match(unsigned int rate)
{
	unsigned int i;

	for (i = 0; i < n_qgv; i++)
		if (abs((int)qgv_rate[i] - (int)rate) * 50 <= qgv_rate[i])
			return qgv_rate[i];
	return 0;
}

static ssize_t mem_clock_mhz_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	unsigned int qclk, rate;
	u64 q;
	int ret;

	ret = pmt_telem_read(pmt_ep, PMT_QCLK_QWORD, &q, 1);
	if (ret)
		return ret;
	qclk = ((q >> PMT_QCLK_SHIFT) & 0xff) * 100 / 3;

	/*
	 * Gear 2 first. Ambiguous only if the table held both X and 2X, which
	 * it does not here (3200/4800/5600/8533).
	 */
	rate = qgv_match(qclk * 4);
	if (!rate)
		rate = qgv_match(qclk * 2);
	if (!rate)
		return -ENODATA;
	return sysfs_emit(buf, "%u\n", rate);
}
static DEVICE_ATTR_RO(mem_clock_mhz);

static struct attribute *mem_attrs[] = {
	&dev_attr_mem_clock_mhz.attr,
	NULL
};
ATTRIBUTE_GROUPS(mem);

static int hw_read(struct device *dev, enum hwmon_sensor_types type,
		   u32 attr, int ch, long *val)
{
	int ret;

	if (type == hwmon_temp) {
		if (attr == hwmon_temp_crit) {
			*val = tjmax_mc;
			return 0;
		}
		return read_gcd_max(val);
	}

	mutex_lock(&lock);
	ret = update();
	/* hwmon energy is in microjoules */
	*val = (long)mul_u64_u32_shr(acc_raw, 1000000, esu);
	mutex_unlock(&lock);
	return ret;
}

static int hw_read_string(struct device *dev, enum hwmon_sensor_types type,
			  u32 attr, int ch, const char **str)
{
	*str = "gpu";
	return 0;
}

static const struct hwmon_channel_info *const info[] = {
	HWMON_CHANNEL_INFO(energy, HWMON_E_INPUT | HWMON_E_LABEL),
	/*
	 * Channel 0 is hidden by is_visible(); it only makes the GT sensor
	 * temp2. Its config can't be 0, which would terminate the list.
	 */
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT,
			   HWMON_T_INPUT | HWMON_T_LABEL | HWMON_T_CRIT),
	NULL
};

static const struct hwmon_ops ops = {
	.is_visible = is_visible,
	.read = hw_read,
	.read_string = hw_read_string,
};

static const struct hwmon_chip_info chip = { .ops = &ops, .info = info };

static void pmt_setup(void)
{
	struct telem_endpoint *ep;
	u64 v;

	if (!rdmsrq_safe(MSR_TEMP_TARGET, &v))
		tjmax_mc = ((v >> 16) & 0xff) * 1000;

	pmt_pdev = pci_get_domain_bus_and_slot(0, 0, PMT_DEV);
	if (!pmt_pdev)
		return;
	ep = pmt_telem_find_and_register_endpoint(&pmt_pdev->dev, PMT_GUID, 0);
	if (IS_ERR(ep)) {
		pr_info("intel_igpu_hwmon: no PMT GUID %#x, temp and mem clock disabled\n", PMT_GUID);
		pci_dev_put(pmt_pdev);
		pmt_pdev = NULL;
		return;
	}
	pmt_ep = ep;
}

static void qgv_setup(void)
{
	void __iomem *mmio;
	unsigned int i, n;
	u32 g;

	/* Reads of a runtime-suspended GPU return all-ones */
	if (pm_runtime_resume_and_get(&gpu->dev) < 0)
		return;
	mmio = pci_iomap_range(gpu, 0, MTL_MEM_SS_INFO_GLOBAL,
			       MTL_QGV_POINT_LOW(MAX_QGV));
	if (!mmio) {
		pm_runtime_put(&gpu->dev);
		return;
	}
	g = readl(mmio);
	n = (g >> 8) & 0xf;
	if (g != ~0u) {
		for (i = 0; i < n; i++)
			qgv_rate[i] = DIV_ROUND_CLOSEST((readl(mmio + MTL_QGV_POINT_LOW(i)) & 0xffff) * 100, 6);
		n_qgv = n;
	}
	pci_iounmap(gpu, mmio);
	pm_runtime_put(&gpu->dev);
}

static void pmt_teardown(void)
{
	if (pmt_ep)
		pmt_telem_unregister_endpoint(pmt_ep);
	pci_dev_put(pmt_pdev);
}

static int __init igpu_hwmon_init(void)
{
	u64 unit, v;

	if (rdmsrq_safe(MSR_UNIT, &unit) || rdmsrq_safe(MSR_PP1, &v))
		return -ENODEV;
	esu = (unit >> 8) & 0x1f;
	last_raw = (u32)v;

	gpu = pci_get_domain_bus_and_slot(0, 0, PCI_DEVFN(2, 0));
	if (!gpu || gpu->vendor != PCI_VENDOR_ID_INTEL) {
		pci_dev_put(gpu);
		return -ENODEV;
	}

	pmt_setup();
	qgv_setup();

	hwmon = hwmon_device_register_with_info(&gpu->dev, "xe", NULL,
						&chip, pmt_ep && n_qgv ? mem_groups : NULL);
	if (IS_ERR(hwmon)) {
		pmt_teardown();
		pci_dev_put(gpu);
		return PTR_ERR(hwmon);
	}
	return 0;
}

static void __exit igpu_hwmon_exit(void)
{
	hwmon_device_unregister(hwmon);
	pmt_teardown();
	pci_dev_put(gpu);
}

module_init(igpu_hwmon_init);
module_exit(igpu_hwmon_exit);
MODULE_DESCRIPTION("Intel iGPU power, temperature and memory clock as hwmon");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("INTEL_PMT_TELEMETRY");
/* The PMT endpoint must exist before pmt_setup() looks it up */
MODULE_SOFTDEP("pre: intel_vsec pmt_telemetry");
