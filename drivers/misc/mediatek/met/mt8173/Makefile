$(info =============================)
$(info Start to build MET for $(MTK_PLATFORM))

$(info MET_TOP=$(MET_TOP))
ifeq ($(MET_TOP),)

ifeq ($(MTK_ROOT_BUILD),)

ccflags-y += -Werror
$(info PATH=$(ccflags-y))
obj-y := core/

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

check-platform-dir-exist := $(wildcard $(srctree)/drivers/misc/mediatek/met/$(MTK_PLATFORM)/platform/$(MTK_PLATFORM)/)
ifneq ($(check-platform-dir-exist),)
	platform-dir := platform/$(MTK_PLATFORM)/
	obj-y += $(platform-dir)
endif

subdir-ccflags-y := -I$(srctree)/drivers/misc/mediatek/met/ -I$(srctree)/drivers/misc/mediatek/met/$(MTK_PLATFORM) -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

else
include $(MTK_ROOT_BUILD)/Makefile

#export CONFIG_MTK_MET=y
ccflags-y += -Werror
obj-y := core/

met-platform := $(call lc,$(MTK_PLATFORM))
platform-dir := platform/$(met-platform)/
check-platform-dir-exist := $(wildcard $(srctree)/mediatek/platform/$(met-platform)/kernel/drivers/met/platform/$(met-platform)/)
ifneq ($(check-platform-dir-exist),)
	obj-y += $(platform-dir)
endif

subdir-ccflags-y := -I$(srctree)/mediatek/platform/$(met-platform)/kernel/drivers/met

ifeq ($(CONFIG_THERMAL),y)
	subdir-ccflags-y +=	-DNO_MTK_THERMAL_GET_TEMP=0
else
	subdir-ccflags-y +=	-DNO_MTK_THERMAL_GET_TEMP=1
endif

endif

else

$(info KERNELRELEASE=$(KERNELRELEASE))
ifneq ($(KERNELRELEASE),)

$(info CONFIG_MTK_MET=$(CONFIG_MTK_MET))
ifeq ($(CONFIG_MTK_MET),)
	obj-y := core/
endif


$(info MET_PLATFORM=$(MET_PLATFORM))
ifneq ($(MET_PLATFORM),)
	obj-y += platform/$(MET_PLATFORM)/
endif
$(info obj-y=$(obj-y))

subdir-ccflags-y := -I$(MET_TOP)/met

else

$(info KDIR=$(KDIR))
ifeq ($(KDIR),)
	$(error Error! You should execute "make" in parent directory)
endif

export KBUILD_OUTPUT_SUPPORT=yes

default:
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

endif

endif

$(info Build MET done!!!)
$(info obj-y=$(obj-y))
$(info =============================)
