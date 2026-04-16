#include "stdint.h"

#ifndef ADXL_H
#define ADXL_H

typedef enum {
	STATE_IDLE,
	STATE_START_SENT,
	STATE_ADDR_SENT,
	STATE_RECEIVING,
	STATE_COMPLETE,
	STATE_ERROR
} I2C_State_t;

extern volatile I2C_State_t currentState;

void TimerStart();
uint8_t I2C_init(void);
uint8_t ADXL345_pwr(void);
uint8_t ADXL345_read();

void ADXL345_StartRead();

#endif
