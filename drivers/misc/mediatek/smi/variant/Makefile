ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ifeq ($(CONFIG_ARCH_MT8173),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
endif

obj-y += smi_variant.o
obj-y += smi_debug.o

ifeq ($(CONFIG_ARCH_MT8173),y)
obj-y += smi_variant_config_8173.o
ccflags-y += -DMT73
obj-y += mmdvfs_mgr_8173.o
endif

ifeq ($(CONFIG_ARCH_MT8127),y)
ccflags-y += -DMT27
obj-y += smi_variant_config_8127.o
endif
