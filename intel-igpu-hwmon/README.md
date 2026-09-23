# intel-igpu-hwmon

xe registers hwmon sensors only for discrete GPUs, so nvtop and btop show no
power or temperature for an integrated one. This module adds an hwmon device
under the iGPU's PCI device.

| Attribute | Source |
|---|---|
| `energy1_input` | RAPL PP1 (graphics) energy MSR, in microjoules |
| `temp2_input` | graphics die maximum temperature from PMT telemetry |
| `temp2_crit` | TjMax |
| `mem_clock_mhz` | DRAM data rate of the active memory QGV point (non-standard) |

The hwmon is named `xe` because btop's xe backend accepts no other name.
Values are read only when a file is read, and the module runs no timers.

## Hardware support

Energy comes from an architectural MSR and should work on any Intel client CPU
with a PP1 domain.

Temperature and memory clock read a PMT telemetry region whose layout Intel has
not published for Arrow Lake-H (GUID `0x1306a0b3`). The offsets come from the
published Meteor Lake layout and were checked against GPU load on an Arrow
Lake-H laptop. With any other GUID the module leaves out both attributes and
still exposes energy.

## Permissions

`energy1_input` is readable by every user. The powercap `energy_uj` files are
root-only because of the PLATYPUS power side channel. PP1 covers the GPU and
not the CPU cores, but think about it before installing this on a shared
machine.

## nvtop and btop

nvtop reads `energy1_input` and `temp2_input` unmodified. btop needs an xe
backend, such as the `btop-intel-git` AUR package. Neither reads
`mem_clock_mhz` without a patch.
