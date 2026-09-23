# gna

Driver for the Intel GNA (Gaussian & Neural Accelerator) as an out-of-tree
module. It registers on the accel subsystem and appears as
`/dev/accel/accelN`.

The code comes from Maciej Kwapulinski's v5 series to dri-devel, which was
never merged, by way of xanderlent's out-of-tree port. Their README is kept
unchanged as [UPSTREAM-README.md](UPSTREAM-README.md). The Fedora packaging
and version scheme it describes do not apply to this repository.

[PORTING-NOTES.md](PORTING-NOTES.md) lists the changes made since that port,
including Arrow Lake-H support, current-kernel API fixes and the move from a
render node to accel. [RE-FINDINGS.md](RE-FINDINGS.md) records what Intel's
Windows driver and direct MMIO probing show about the hardware.

Intel archived the userspace library, [intel/gna](https://github.com/intel/gna),
in May 2025, so the stack above this driver is end-of-life.
