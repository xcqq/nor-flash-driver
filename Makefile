
#KSRC := 
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

obj-m := spiflash.o

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules

.PHONY: clean
clean:
	rm *.o *.ko

	