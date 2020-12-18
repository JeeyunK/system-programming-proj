# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y


# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSBULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I..

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= sbull.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions quiz1 key change_mode

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

quiz1:
	gcc quiz1.c -o quiz1

key : 
	gcc key.c -o key

change_mode :
	gcc change_mode.c -o change_mode
