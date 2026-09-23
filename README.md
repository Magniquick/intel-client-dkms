# intel-client-dkms

Out-of-tree kernel modules for Intel client platforms, packaged for DKMS.

| Directory | Module | AUR package |
|---|---|---|
| [gna/](gna) | `gna` | `intel-gna-dkms-git` |
| [intel-igpu-hwmon/](intel-igpu-hwmon) | `intel_igpu_hwmon` | `intel-igpu-hwmon-dkms-git` |

Both are tested on an Arrow Lake-H laptop (Core Ultra 9 285H) with kernels 7.2
and 6.18.

## Install

On Arch, use the AUR packages. Their PKGBUILDs are kept in [aur/](aur), and
each `pkgver` counts only the commits that touch its own directory.

With plain DKMS, copy a module directory into `/usr/src`, fill in the version
and install it:

    sudo cp -r intel-igpu-hwmon /usr/src/intel-igpu-hwmon-1.0
    sudo sed -i 's/@PKGVER@/1.0/' /usr/src/intel-igpu-hwmon-1.0/dkms.conf
    sudo dkms install intel-igpu-hwmon/1.0

## License

GPL-2.0-only. See the COPYING file in each module directory.
