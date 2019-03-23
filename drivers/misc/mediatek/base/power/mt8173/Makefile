#ccflags-y += -I$(srctree)/drivers/misc/mediatek/irq/$(MTK_PLATFORM)/
#obj-y += mt_pm_init.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt8173
ccflags-y += -I$(srctree)/drivers/regulator
obj-y += mt_wakeup.o
obj-y += mt_spm.o
obj-y += mt_spm_internal.o
obj-$(CONFIG_ARCH_MT8173) += mt_sleep.o
obj-$(CONFIG_ARCH_MT8173) += mt_spm_sleep.o
obj-$(CONFIG_ARCH_MT8173) += mt_spm_dpidle.o
obj-$(CONFIG_ARCH_MT8173) += mt_spm_sodi.o
obj-$(CONFIG_ARCH_MT8173) += mt_spm_fs.o
obj-y += mt_cpuidle.o
obj-y += cpu_dormant.o

obj-y += mt_static_power.o
obj-y += mt_cpufreq.o
obj-y += mt_gpufreq.o
obj-y += mt_cpufreq_tlp.o
ifneq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
obj-y += mt_vcore_dvfs_64.o
obj-y += mt_spm_ddrdfs_64.o
endif
obj-y += mt_pm_init.o

obj-y += mt_dcm.o
obj-y	+= mtcmos.o
obj-$(CONFIG_MTK_PSCI)	+= mt_psci.o
obj-y += mt_hotplug_strategy_main.o mt_hotplug_strategy_core.o mt_hotplug_strategy_procfs.o mt_hotplug_strategy_cpu.o mt_hotplug_strategy_api.o mt_hotplug_strategy_algo.o
obj-$(CONFIG_ARCH_MT8173) += mt_idle.o
obj-$(CONFIG_MTK_BATTERY_THROTTLING) += mt_battery_throttle.o
obj-$(CONFIG_DEBUG_FS) += mt_clkdbg.o
obj-y			  += mt_ptp.o
