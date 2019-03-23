/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

//********************************************************************************
//
//		LC89821x Initialize header 
//
//	    Program Name	: AfInit.h
//		Design			: Rex.Tang
//		History			: First edition						2013.07.20 Rex.Tang
//
//		Description		: Interface Functions and Definations
//********************************************************************************
#ifndef	__AFINIT__
#define __AFINIT__

extern 	void AfInit( unsigned char hall_bias, unsigned char hall_off );

extern	void ServoOn(void);

#endif	/* __AFINIT__ */