
#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifeq ($(CONFIG_MTK_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ext_disp/  \
             -I$(srctree)/drivers/misc/mediatek/video/     \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/ \
             -I$(srctree)/drivers/misc/mediatek/sync/ \
             -I$(srctree)/drivers/misc/mediatek/mhl/driver/mhl_tx_drv/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/hv1/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/hv1/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)/inc/\
             -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)/\
             -I$(srctree)/drivers/staging/android/ion/ \
             -I$(srctree)/drivers/staging/android/ion/mtk/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/lcm/inc/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
#cmdq driver header need followings for secure case
ccflags-y += -I$(srctree)/include/trustzone
ccflags-y += -I$(srctree)/include
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),nxp_tda19989)
	ccflags-y +=	-DHDMI_TDA19989
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),Sii8338)
	ccflags-y +=	-DMHL_SII8338
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),Sii8348)
	ccflags-y +=	-DMHL_SII8348
endif

ifeq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

ifeq ($(CONFIG_SLIMPORT_COLORADO3), y)
ccflags-y += -DMTK_DP_SUPPORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/anx7814/
endif


obj-y	:= extd_ddp.o extd_debug.o  extd_drv.o extd_utils.o extd_lcm.o extd_fence.o

else
obj-y := dummy.o
endif
