
ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

ccflags-y += -I$(src)/../../include/

obj-$(CONFIG_SSUSB_MU3_PHY) := mu3phy.o

mu3phy-y := mtk-phy.o
mu3phy-$(CONFIG_SSUSB_PROJECT_PHY) += mtk-phy-asic.o
