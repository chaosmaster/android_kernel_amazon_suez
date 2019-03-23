MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat


ifeq ($(CONFIG_ARCH_MTK_PROJECT), "suez")
       ccflags-$(CONFIG_MTK_SMART_BATTERY) += -DARCH_MTK_PROJECT_SUEZ
endif

obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_meter.o mt_battery_common.o mt_switch_charging.o mt_bm_data.o

ifeq ($(CONFIG_BATTERY_DTS_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_dts_support.o
endif

ifeq ($(CONFIG_MTK_BQ24297_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24297.o
endif

ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24297.o
endif

ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24196.o
endif

ifeq ($(CONFIG_MTK_BQ24261_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24261.o
endif

ifeq ($(CONFIG_MTK_BQ25890_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq25890.o
endif

ifeq ($(CONFIG_MTK_BQ25896_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq25890.o
endif

obj-$(CONFIG_MTK_PMIC) += mt6397_fgadc.o

ifeq ($(CONFIG_CHARGER_SMB1351), y)
    obj-$(CONFIG_CHARGER_SMB1351)    += smb1351_charger.o

    KBUILD_CFLAGS += -DCONFIG_SMB1351=1
endif
