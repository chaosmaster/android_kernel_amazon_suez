#ifndef MT8173_TOP_IO_H
#define MT8173_TOP_IO_H
#include <asm/io.h>
#include <linux/spinlock.h>
#include <asm-generic/bitops/builtin-ffs.h>

extern spinlock_t msdc_top_lock;

static inline u32 top_sdr_read32(void __iomem *reg)
{
	u32 val;

	spin_lock(&msdc_top_lock);
	val = readl(reg);
	spin_unlock(&msdc_top_lock);

	return val;
}

static inline void top_sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val;

	spin_lock(&msdc_top_lock);
	val = readl(reg);
	val |= bs;
	writel(val, reg);
	spin_unlock(&msdc_top_lock);
}

static inline void top_sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val;

	spin_lock(&msdc_top_lock);
	val = readl(reg);
	val &= ~bs;
	writel(val, reg);
	spin_unlock(&msdc_top_lock);
}

static inline void top_sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv;

	spin_lock(&msdc_top_lock);
	tv = readl(reg);
	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
	spin_unlock(&msdc_top_lock);
}

static inline void top_sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv;

	spin_lock(&msdc_top_lock);
	tv = readl(reg);
	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
	spin_unlock(&msdc_top_lock);
}

#endif
