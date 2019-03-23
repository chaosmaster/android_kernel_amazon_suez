subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/accdet/$(MTK_PLATFORM)
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

ifeq ($(CONFIG_ASUS_MT8173_ACCDET), y)
obj-$(CONFIG_ASUS_MT8173_ACCDET) += $(CONFIG_MTK_PLATFORM)/
else
obj-$(CONFIG_MTK_ACCDET) := accdet_drv.o
obj-$(CONFIG_MTK_ACCDET) += $(CONFIG_MTK_PLATFORM)/
endif

