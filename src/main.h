/*
 /*
 * main.h
 *
 *  Created on: 23 abr. 2021
 *      Author: agust
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include <string.h>
#include "../BSP/bsp.h"

typedef enum{
	eSender1,
	eSender2
} DataSource_t;
/* Define the structure type that will be passed on the queue. */
typedef struct
{
	uint8_t ucValue;
	DataSource_t eDataSource;
} Data_t;



void SW_PressEvent(void);


#endif /* MAIN_H_ */
