# intel-gna DKMS

Intel GNA (Gaussian & Neural Accelerator) DRM driver, packaged for DKMS.

## Provenance

Maciej Kwapulinski's v5 patch series to dri-devel, which was never merged:
https://lore.kernel.org/all/20221020175334.1820519-1-maciej.kwapulinski@linux.intel.com/

Series history: v1 (12 patches, 2021-02-16) -> v2 (13) -> v3 (14) -> v4 (10)
-> v5 (10, 2022-10-20). Last list activity 2023-01-24. Never applied.

Out-of-tree scaffolding derives from xanderlent/intel-gna-kmod, which
forward-ported v5 to Linux 6.9 and packaged it as a Fedora kmod RPM. This
package continues that to current kernels and adds DKMS + Arrow Lake.

## Changes made here

1. Arrow Lake-H support. GNA_DEV_HWID_ARL 0x774C, GEN2 features (matching
   MTL and RPL, its nearest neighbours). Upstream v5 stopped at MTL 0x7E4C.

2. struct drm_driver::date removed. cb2e1c2136f7, v6.14.

3. drm_gem_shmem_put_pages() -> _locked(), v6.16. The caller must now hold
   the reservation lock, so the call moved above dma_resv_unlock() rather
   than below it. This also closes a small window in the original.

4. pm_runtime_put() returns void rather than int, v7.0.

5. Dropped the vendored drm_internal.h. It existed only to reach
   drm_gem_vmap()/vunmap(), which are public in <drm/drm_gem.h> now. Its
   stale drm_minor_acquire() prototype conflicted with the current public
   one, which gained an xarray argument.

6. include/uapi/drm/gna_drm.h: "drm.h" -> <drm/drm.h>, since the relative
   form only resolves in-tree.

7. Registers on the accel subsystem instead of as a DRM render node.
   Upstream v5 sets `DRIVER_GEM | DRIVER_RENDER`, which creates
   /dev/dri/card1 and /dev/dri/renderD129. GNA is an inference accelerator,
   not a GPU, so those nodes sit in front of everything that enumerates
   /dev/dri looking for one: Mesa logs "Driver does not support the 0x774c
   PCI ID" and Chromium-based browsers pick the node up and lose hardware
   rendering. Now `DRIVER_GEM | DRIVER_COMPUTE_ACCEL` with
   DEFINE_DRM_ACCEL_FOPS, so it appears as /dev/accel/accelN and /dev/dri is
   left to the GPU.

   This is not a mistake in the original. drivers/accel and
   DRIVER_COMPUTE_ACCEL landed in v6.2, a month after the v5 series went
   quiet, so there was nothing else to register as at the time. Intel's own
   NPU driver, ivpu, uses accel and takes accel0 on this machine.

   Guarded by GNA_HAVE_DRM_ACCEL. `accel_open` is declared only under
   `IS_ENABLED(CONFIG_DRM_ACCEL)`, so the compile probe fails both on
   kernels too old to have the subsystem and on kernels built without it,
   and those keep the render node.

   Verified on 7.2.3-1-cachyos: /dev/accel/accel1 at 0666, DRM_IOCTL_VERSION
   reports "gna", and all seven GNA_GET_PARAMETER values read correctly as an
   unprivileged user, so DRM_RENDER_ALLOW ioctls work on an accel minor.

   Compatibility note for the archived userspace: intel/gna scans
   /dev/dri/renderD*. A symlink there pointing at the accel node works
   (tested: DRM_IOCTL_VERSION and GNA_GET_PARAMETER both succeed through it),
   so an old libgna can still be pointed at the device without giving the
   render node back to Mesa and Chromium.

8. `gna_pm_init` ends with `pm_runtime_put_autosuspend()` rather than
   `pm_runtime_put_noidle()`. The original drops the probe-time reference
   without running an idle check, so nothing schedules the first autosuspend.
   This is the documented idiom and is correct on its own terms.

   **It does not fix the reload case, and was not verified to change any
   observed behaviour.** Do not treat it as a fix. See the open question
   below.

## Open: GNA stays in D0 after a module reload

Loaded at boot the device behaves: 10773 s suspended against 1485 s active on
one uptime. After a manual `modprobe -r gna; modprobe gna` it never suspends
again. Measured: `runtime_active_time` climbs 10003 ms per 10 s wall and
`runtime_suspended_time` does not move, `runtime_status` is `active`,
`power_state` is `D0`. Forcing an idle check by toggling `power/control`
`on` then `auto` does not shift it either, so a usage reference is held
rather than an idle check merely being missed.

Runtime PM lives on the PCI device (`gna_pci.c:165` sets `.pm`, and
`gna_probe` passes the PCI device as `parent`). That device is not destroyed
by `rmmod`, only unbound, so its `usage_count` persists across a reload.
Suspect pairing: `gna_pm_init` calls `pm_runtime_allow()`, which only
decrements the first time because `runtime_auto` is already true afterwards,
while `gna_pm_fini` calls `pm_runtime_get_noresume()` on every unload and
nothing calls `pm_runtime_forbid()`. That accounting was not confirmed.

Not settled because this kernel has neither `CONFIG_PM_ADVANCED_DEBUG` (no
`power/runtime_usage` to read) nor bpftrace installed. Either would answer it
in one look.

Self-heals on reboot, since the count starts fresh. Cost while stuck is one
small IP block held in D0, not measured.

Related: a single `Runtime PM usage count underflow!` appeared on the first
unload of the boot-loaded module and has not recurred across roughly six
later cycles, which is consistent with the count being wrong by one from that
point on.

## Feature detection, not version checks

conftest.sh probes the target kernel by compiling snippets, because DKMS
rebuilds against kernels that did not exist when this was written and
distributions backport APIs, which makes LINUX_VERSION_CODE unreliable.

It self-tests with a positive and a negative control and aborts if either
gives the wrong answer. Without that, a probe that fails for an unrelated
reason (a missing MODULE_LICENSE at modpost, say) silently reports every
feature as absent.

## Userspace

The library is https://github.com/intel/gna, archived by Intel on 2025-05-05.
OpenVINO dropped its GNA plugin after 2023.3. So the kernel side works, but
the userspace stack above it is end-of-life.

## Licence

GPL-2.0-only, per the original Intel source. Not relicensable.
