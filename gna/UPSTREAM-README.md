Intel GNA DRM driver for Linux
==============================

(as an out-of-tree kernel module)
---------------------------------

This is the Linux kernel driver for the Intel GNA hardware. A corresponding [user space library](https://github.com/intel/gna) provides an API for using the hardware, which I have also packaged in [xanderlent/intel-gna-rpm](https://github.com/xanderlent/intel-gna-rpm).

This code is derived from Intel engineer Maciej Kwapulinski's v5 patch series to the dri-devel mailing list, available [at kernel.org's lore public-inbox instance](https://lore.kernel.org/dri-devel/20221020175334.1820519-1-maciej.kwapulinski@linux.intel.com/), and [which applies cleanly to Linux v6.0](https://github.com/xanderlent/linux/tree/intel-gna-patches-v5-on-linux-v6.0).

I [forward-ported the patch series to Linux v6.9](https://github.com/xanderlent/linux/tree/intel-gna-patches-v5-forward-ported-linux-v6.9), fixing the code with respect to a change in the internal DRM GEM API and making sure that every patch in the series builds cleanly. I then ported the code to build as an out-of-tree module, and then packaged it with the Fedora/RPMFusion kmod v2 framework to build kmod RPMs.

### Kernel Header Versioning

Note that while this kernel module installs the user space API header `gna_drm.h`, the Intel GNA library source code includes its own copy of the `gna_drm.h` header, and **is not** built against the system version of that header provided by this kernel module. That means the library will need to be manually patched to keep things in sync if changes are needed.

### About the version number

The version number consists of `<upstream_patch_version>.<downstream_changeset_version>-<release1>.<release2>`, which generates package versions like `5.1-1.fc40.1` since the RPM spec build the package for a specific distribution.

You should expect that the major number will be 5 and the minor version will be 1, because it is based on the upstream patchset number and the downstream changeset version. The release1 number will likely continue to be 1 as well, since it will be incremented only if changes to the out-of-tree build process are needed. The the release2 number will be incremented as needed for changes to the RPM spec, README.md, or other support files, or for mass rebuilds as needed.

### Package Availability

[![Copr build status](https://copr.fedorainfracloud.org/coprs/xanderlent/intel-gna-driver/package/intel-gna-kmod/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/xanderlent/intel-gna-driver/package/intel-gna-kmod/)

This package is available for use with Fedora Linux and possibly other RPM-based distributions through my Fedora Copr repository, [xanderlent/intel-gna-driver](https://copr.fedorainfracloud.org/coprs/xanderlent/intel-gna-driver). See that page for information on how to install and use this software on Fedora Linux (and possibly other RPM-based distributions).
