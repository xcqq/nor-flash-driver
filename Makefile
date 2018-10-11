ifneq ($(KERNELRELEASE),)
obj-m := spiflash.o
else
KSRC := ~/sourcecode/allwinner/nanopi-wolf/mainline/kernel
ARCH := 
CROSS_COMPILE := 

all: modules

modules:
	$(MAKE) -C $(KSRC) M=$(shell pwd) modules

.PHONY: clean
clean:
	rm *.o *.ko	
endif