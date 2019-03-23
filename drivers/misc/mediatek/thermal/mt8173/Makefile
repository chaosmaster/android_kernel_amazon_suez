#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
obj-  := dummy.o

ifeq ($(CONFIG_THERMAL), y)
else
    obj-y += mtk_thermal_dummy.o
endif

obj-$(CONFIG_THERMAL) += mtk_thermal_platform.o

# Only included in non-EVB projects
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
#obj-$(CONFIG_THERMAL) += tmp103_temp_sensor.o
#obj-$(CONFIG_THERMAL) += mtk_ts_bts_tmp103.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_pmic.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_wmt.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_battery.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_cpu.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_gpu.o
obj-$(CONFIG_THERMAL) += mtk_temp_thermistor.o
obj-$(CONFIG_THERMAL) += tm_nth.o
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
obj-$(CONFIG_THERMAL) += mtk_thermal_budget.o
