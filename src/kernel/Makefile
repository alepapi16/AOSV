obj-m := groups.o

PWD = $(shell pwd)
KERNELDIR = /lib/modules/$(shell uname -r)/build/
ccflags-y += -I$(KINC)

modules:
	@printf '$(bold)************  BUILDING KERNEL MODULE ************\n$(sgr0)'
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	mkdir $(OBJ)/kernel
	rm -f *.o .*.cmd *.mod.c *.mod modules.order *.symvers
	mv -t $(OBJ)/kernel *.ko