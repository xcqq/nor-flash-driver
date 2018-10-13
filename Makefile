ifneq ($(KERNELRELEASE),)
obj-m := spiflash.o
else
KSRC := ~/sourcecode/allwinner/nanopi-wolf/mainline/kernel
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

all: 
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules

.PHONY: clean
clean:
	rm *.o *.ko	
endif