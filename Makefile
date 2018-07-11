MODULE_1    = ckun
MODULE_2    = multiq
KERNEL_DIR  = /lib/modules/$(shell uname -r)/build
PWD         = $(shell pwd)

#ccflags-y		= -std=gnu99

obj-m			:= $(MODULE_1).o
$(MODULE_1)-y		+= main.o

obj-m			+= $(MODULE_2).o
$(MODULE_2)-y		+= mq.o


all: 
	make -C $(KERNEL_DIR) M=$(PWD) modules

install:
	make -C $(KERNEL_DIR) M=$(PWD) modules_install

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean