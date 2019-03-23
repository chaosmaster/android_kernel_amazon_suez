# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ft5726/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += focaltech_driver.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += focaltech_ctl.o

obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += Config_FT5822.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += DetailThreshold.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += Global.o ini.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += Test_FT5822.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FT5726) += test_lib.o
