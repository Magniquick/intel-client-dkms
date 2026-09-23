#!/bin/sh
# Build intel_igpu_hwmon against an arbitrary kernel, picking the toolchain from that
# kernel rather than from the running one.
#
# DKMS builds for kernels that are not booted, and this machine has a clang
# CachyOS kernel installed alongside a gcc Arch one. Guessing from `uname -r`
# builds one with the other's compiler, and every symbol then mismatches.
# Consult the first config that exists and stop, so "not clang" is never
# confused with "no config found".
set -eu
KDIR="$1"; BUILD="$2"; shift 2

LLVM=""
for cfg in "$KDIR/.config" "$KDIR/../source/.config" "/proc/config.gz"; do
	[ -e "$cfg" ] || continue
	case "$cfg" in
		*.gz) zcat "$cfg" 2>/dev/null | grep -q "^CONFIG_CC_IS_CLANG=y" && LLVM=1 ;;
		*)    grep -q "^CONFIG_CC_IS_CLANG=y" "$cfg" 2>/dev/null && LLVM=1 ;;
	esac
	break
done

echo "intel_igpu_hwmon: building against $KDIR with ${LLVM:+LLVM=1}${LLVM:+ (clang)}$([ -z "$LLVM" ] && echo gcc)"
exec make -C "$KDIR" M="$BUILD" ${LLVM:+LLVM=1} "$@"
