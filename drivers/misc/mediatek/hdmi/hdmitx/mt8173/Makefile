# include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/staging/android \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)/inc/

obj-y := hdmi_drv.o hdmi_ctrl.o hdmiavd.o hdmicec.o hdmiddc.o hdmiedid.o hdmihdcp.o hdmicmd.o hdmictrl.o hdmi_debug.o

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/include/trustzone/
obj-y += hdmi_ca.o
endif

ifeq ($(CONFIG_SLIMPORT_ANX3618),y)
ccflags-y += -I$(srctree)/drivers/misc/analogix
endif
endif

ifeq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

ifeq ($(CONFIG_SLIMPORT_COLORADO3), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/anx7814/
endif