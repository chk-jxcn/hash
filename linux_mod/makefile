ifneq ($(KERNELRELEASE),)
obj-m:=hash_mod.o
else
KERNELDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR)  M=$(PWD) modules
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko Module.symvers modules.order hash_mod.ko.*
endif
