obj-m := intel_igpu_hwmon.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	sh build.sh $(KDIR) $(CURDIR) modules

clean:
	sh build.sh $(KDIR) $(CURDIR) clean
