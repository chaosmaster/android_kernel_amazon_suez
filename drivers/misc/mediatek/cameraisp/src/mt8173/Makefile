#
# Makefile for misc devices that really don't fit anywhere else.
#

# for m4u.h , cmdq_core.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1/$(MTK_PLATFORM)

obj-y += camera_isp.o
obj-y += camera_fdvt.o
obj-y += camera_sysram.o

$(info cameraisp drv by platform $(platform_drv))
