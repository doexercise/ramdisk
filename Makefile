TARGET_NAME = ckun
KERNEL_DIR  = /lib/modules/$(shell uname -r)/build
PWD         = $(shell pwd)

#ccflags-y		= -std=gnu99

obj-m			= $(TARGET_NAME).o
obj-m			= multiq.o
$(TARGET_NAME)-objs	= main.o
multiq-y		+= mq.o


all: 
	make -C $(KERNEL_DIR) M=$(PWD) modules

install:
	make -C $(KERNEL_DIR) M=$(PWD) modules_install

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean