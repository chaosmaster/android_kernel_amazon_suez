#ifndef _IDME_H_
#define _IDME_H_
#include "board_id.h"
unsigned int idme_get_board_rev(void);
unsigned int idme_get_board_type(void);
unsigned int idme_get_battery_info(int index, size_t length);
#endif
