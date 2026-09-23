// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2017-2022 Intel Corporation */
/* Register visibility for out-of-tree GNA, added downstream.
 *
 * The upstream v5 driver reads exactly one value out of the device
 * (GNA_MMIO_IBUFFS, masked to its low 8 bits) and takes everything else
 * from static per-SKU tables. On Arrow Lake-H (0x774C) the hardware
 * carries stable, non-zero values in registers no public GNA driver maps:
 * 0x088, and a block at 0x100..0x10C. IBUFFS itself holds more than the
 * low byte the driver keeps.
 *
 * Their meaning is not documented anywhere public, so this only exposes
 * them for observation. Nothing here interprets or acts on them.
 */

#include <linux/debugfs.h>
#include <linux/pci.h>
#include <linux/seq_file.h>

#include "gna_device.h"
#include "gna_hw.h"

static const struct {
	u32 off;
	const char *name;
	const char *note;
} gna_regs[] = {
	{ GNA_MMIO_STS,     "STS",     "status" },
	{ GNA_MMIO_CTRL,    "CTRL",    "control" },
	{ 0x88,             "?0x088",  "undocumented; not in any public map" },
	{ GNA_MMIO_PTC,     "PTC",     "perf: total cycles" },
	{ GNA_MMIO_PSC,     "PSC",     "perf: stall cycles" },
	{ GNA_MMIO_0xA0,    "?0x0a0",  "gna.sys zeroes on STS bit 1; unnamed upstream" },
	{ GNA_MMIO_0xA4,    "?0x0a4",  "gna.sys zeroes on STS bit 1; unnamed upstream" },
	{ GNA_MMIO_D0I3C,   "D0I3C",   "D0i3 control" },
	{ GNA_MMIO_DESBASE, "DESBASE", "descriptor base" },
	{ GNA_MMIO_IBUFFS,  "IBUFFS",  "driver uses bits 7:0 only" },
	{ 0x100,            "?0x100",  "undocumented block" },
	{ 0x104,            "?0x104",  "undocumented block" },
	{ 0x108,            "?0x108",  "undocumented block" },
	{ 0x10c,            "?0x10c",  "undocumented block" },
};

static int gna_regs_show(struct seq_file *s, void *unused)
{
	struct gna_device *gna_priv = s->private;
	int i;

	seq_printf(s, "%-8s %-6s %-10s %s\n", "offset", "name", "value", "note");
	for (i = 0; i < ARRAY_SIZE(gna_regs); i++) {
		u32 v = gna_reg_read(gna_priv, gna_regs[i].off);

		seq_printf(s, "0x%03x    %-6s 0x%08x %s\n",
			   gna_regs[i].off, gna_regs[i].name, v, gna_regs[i].note);
	}

	seq_puts(s, "\n");
	seq_printf(s, "hwid            0x%04x\n", gna_priv->info.hwid);
	seq_printf(s, "in_buf_s        %u (IBUFFS bits 7:0, as used by the driver)\n",
		   gna_priv->hw_info.in_buf_s);
	seq_printf(s, "hw_ver          0x%02x (GNA %u.%u, from IBUFFS bits 31:24)\n",
		   gna_priv->hw_info.hw_ver,
		   gna_priv->hw_info.hw_ver / 10, gna_priv->hw_info.hw_ver % 10);
	seq_printf(s, "max_layer_count %u (static table, not read from HW)\n",
		   gna_priv->info.max_layer_count);
	seq_printf(s, "max_hw_mem      %llu (static table, not read from HW)\n",
		   gna_priv->info.max_hw_mem);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(gna_regs);

/* Own debugfs root rather than the DRM device's: DRM's debugfs plumbing has
 * churned repeatedly (drm_device::debugfs_entry came and went), and this
 * module has to keep building against future kernels under DKMS.
 */
static struct dentry *gna_debugfs_root;

void gna_debugfs_init(struct gna_device *gna_priv)
{
	struct pci_dev *pdev = to_pci_dev(gna_priv->drm.dev);

	if (!gna_debugfs_root)
		gna_debugfs_root = debugfs_create_dir("gna", NULL);
	if (IS_ERR_OR_NULL(gna_debugfs_root))
		return;

	gna_priv->debugfs = debugfs_create_dir(pci_name(pdev), gna_debugfs_root);
	if (IS_ERR_OR_NULL(gna_priv->debugfs))
		return;

	debugfs_create_file("registers", 0444, gna_priv->debugfs, gna_priv,
			    &gna_regs_fops);
}

void gna_debugfs_fini(struct gna_device *gna_priv)
{
	debugfs_remove_recursive(gna_priv->debugfs);
	gna_priv->debugfs = NULL;
}

void gna_debugfs_exit(void)
{
	debugfs_remove_recursive(gna_debugfs_root);
	gna_debugfs_root = NULL;
}
