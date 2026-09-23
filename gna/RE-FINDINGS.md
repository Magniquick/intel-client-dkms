# GNA: what the hardware and Intel's Windows driver know that upstream doesn't

Sources: Intel's Windows driver gna.sys 03.05.00.1578 (11/14/2023) and gna.inf,
taken from the Windows partition on this machine; the archived userspace library
github.com/intel/gna; and direct MMIO probing of 8086:774C on a Yoga Pro 7
14IAH10 (Core Ultra 9 285H, Arrow Lake-H).

## Confirmed and implemented

**Device IDs.** gna.inf lists three that the v5 Linux series never had:
0x774C (GNA 3.5, this machine), 0xAE4C (GNA 3.5), 0x467E (GNA 3.0). Added.
Intel's INF independently confirms 0x774C is GNA 3.5, so the device-type
mapping is not a guess.

**IBUFFS bits 31:24 are a hardware version.** Encoded as major*10 + minor;
this machine reads 0x23, i.e. GNA 3.5, matching the INF. Upstream masks IBUFFS
to bits 7:0 and discards the rest. gna.sys branches on this field being
greater than 0x22. Now decoded into gna_hw_info.hw_ver and exposed through
GNA_PARAM_HW_VER and debugfs.

**Abort acknowledge on GNA 3.5+.** gna.sys sets CTRL bit 2 and then polls CTRL
until the bit self-clears, gated on the version field above; upstream sets the
bit and goes straight to polling STS. Verified on hardware: writing CTRL |= BIT(2)
on an idle device reads back clear on the first poll, so the bit is genuinely
self-clearing here. Added, version-gated. Note this is not a bug fix I have
observed: on an idle device it clears immediately, so it would only matter when
aborting a computation that is actually running.

**Parameter set.** GnaDrvApi.h shows the Windows driver exposing eleven
parameters against upstream's four. Added the three that have a real source:
DEVICE_ID (from the PCI ID), HW_VER (from IBUFFS), HAS_MMU (gna_mmu_init runs
unconditionally). Appended as ids 5..7 so the existing four keep their values.

## Found, deliberately not implemented

**STS bit 1, STS bit 16, CTRL bit 9, MMIO 0xA0 and 0xA4.** gna.sys's ISR decodes
STS bits 0, 1, 4-8, 16 and 17. On bit 1 it clears CTRL bit 9 and zeroes 0xA0 and
0xA4. Upstream defines none of these, and its ISR does no status decoding at all:
it clears dev_busy, wakes the waitqueue and returns. Reaching parity here is a
rewrite of the interrupt path, and I cannot trigger the conditions without a
working userspace client, so the bits are defined and exposed but nothing acts
on them. Implementing untested interrupt handling would be guesswork.

**CE_NUM, PLE_NUM, AFE_NUM, QOS_HARD_TIMEOUT_MS.** Exposed by Windows. I found
no hardware register that sources the engine counts, and the library's
per-generation tables are template-derived rather than tabulated. Not added
rather than invented.

## Negative result worth recording

MMIO 0x100-0x10C reads stable and non-zero (0x0120021f, 0x00000e00, 0x01200200,
0x00000400) and appears in no public register map. gna.sys does not touch it
either: the apparent hits at that offset in the disassembly are [rbp+0x100]
stack frame accesses, not MMIO. So it is dark to both drivers, not a parity gap.
0x088 likewise reads 0x00000100 and is used by neither.

All four are visible in debugfs so the question stays open rather than lost.

## Method

Registers were dumped by unbinding the driver and mmap'ing resource0, then
sampled repeatedly: stable across samples means capability or configuration,
varying means status. All of the above were stable. Windows-side offsets were
confirmed as MMIO rather than driver-struct offsets by checking the same base
register was used for an offset already known to be a register, and by
decompiling the containing function.
