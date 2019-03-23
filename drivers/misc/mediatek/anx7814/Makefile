#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_SLIMPORT_COLORADO3)	+= co3.o
co3-objs := slimport.o \
	    slimport_tx_drv.o