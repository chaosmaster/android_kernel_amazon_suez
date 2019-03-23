#$(info [CLS] MTK_PLATFORM=$(MTK_PLATFORM) )
#$(info [CLS] CONFIG_FTRACE=$(CONFIG_FTRACE))
#$(info [CLS] CONFIG_TRACING=$(CONFIG_TRACING))
#$(info [CLS] CONFIG_ARCH_MT6735M=$(CONFIG_ARCH_MT6735M))
#$(info [CLS] CONFIG_ARCH_MT6735=$(CONFIG_ARCH_MT6735))

check-platform-dir-exist := $(wildcard $(srctree)/drivers/misc/mediatek/met/$(MTK_PLATFORM))
ifneq ($(check-platform-dir-exist),)
ifneq ($(MTK_PLATFORM), elbrus)
ifeq ($(CONFIG_FTRACE),y)
ifeq ($(CONFIG_TRACING),y)
	obj-y += $(MTK_PLATFORM)/
endif
endif
endif
endif

obj-y += temp.o
