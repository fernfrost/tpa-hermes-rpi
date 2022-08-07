export KDIR = /lib/modules/$(shell uname -r)/build
export INSTALL_MOD_DIR = kernel

obj-m += sound/soc/bcm/
subdir-y += arch/arm/boot/dts/overlays/

PHONY := all
all: build

PHONY += build
build:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

PHONY += install
install: build
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	sudo cp arch/arm/boot/dts/overlays/tpa-hermes-rpi.dtbo /boot/overlays

PHONY += clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


.PHONY = $(PHONY)