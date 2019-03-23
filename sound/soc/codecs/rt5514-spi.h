/*
 * rt5514-spi.h  --  ALC5514 driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT5514_SPI_H__
#define __RT5514_SPI_H__

#define RT5514_SPI_BUF_LEN		240
#define RT5514_BUFFER_VOICE_BASE	0x18000200
#define RT5514_BUFFER_VOICE_LIMIT	0x18000204
#define RT5514_BUFFER_VOICE_RP		0x18000208
#define RT5514_BUFFER_VOICE_WP		0x1800020c
#define RT5514_REC_RP			0x18001034
#define RT5514_RW_ADDR_START		0x4ff60000
#define RT5514_RW_ADDR_END		0x4ffb8000
#define RT5514_RW_ADDR_START_2		0x4ffc0000
#define RT5514_RW_ADDR_END_2		0x4ffd0000

/* SPI Command */
enum {
	RT5514_SPI_CMD_16_READ = 0,
	RT5514_SPI_CMD_16_WRITE,
	RT5514_SPI_CMD_32_READ,
	RT5514_SPI_CMD_32_WRITE,
	RT5514_SPI_CMD_BURST_READ,
	RT5514_SPI_CMD_BURST_WRITE,
};

int rt5514_spi_read_addr(unsigned int addr, unsigned int *val);
int rt5514_spi_write_addr(unsigned int addr, unsigned int val);
int rt5514_spi_burst_read(unsigned int addr, u8 *rxbuf, size_t len);
int rt5514_spi_burst_write(u32 addr, const u8 *txbuf, size_t len);
void reset_pcm_read_pointer(void);
void set_pcm_is_readable(int readable);
void rt5514_SetRP_onIdle(void);
#endif /* __RT5514_SPI_H__ */
