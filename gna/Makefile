# SPDX-License-Identifier: GPL-2.0-only
ifneq ($(KERNELRELEASE),)
obj-m += gna.o
gna-y := gna_device.o gna_hw.o gna_ioctl.o gna_mem.o gna_pci.o gna_request.o gna_score.o gna_debugfs.o
ccflags-y += -I$(src) -I$(src)/include
else
KDIR ?= /lib/modules/$(shell uname -r)/build

# Probe the target kernel before building; see conftest.sh.
gna_config.h:
	@sh $(CURDIR)/conftest.sh $(KDIR) $(CURDIR)/gna_config.h

modules default: gna_config.h
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f gna_config.h
.PHONY: modules default clean
endif
