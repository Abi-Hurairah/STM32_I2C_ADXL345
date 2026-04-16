#include "adxl.h"
#include "stdint.h"
#include "stm32f4xx_it.h"

#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"

void I2C1_EV_IRQHandler(void){
	// read SR1 register to capture snapshot of the hardware
	uint32_t sr1 = I2C1->SR1;

	switch (currentState){
		case STATE_START_SENT:
			if (sr1 & (1U << 0)){
				// write the address to SDA bus via data register
				I2C1 -> DR = (0x53 << 1);
				currentState = STATE_ADDR_SENT;
			}
	}
}

void TimerStart(){
	// Set to 500 milisecond
	SysTick->LOAD = 16000 * 500 - 1; // cycles per MS = 16000

	// Reset the clock value
	SysTick-> VAL = 0;

	// Enable and set clock source to processor clock
	SysTick -> CTRL |= (1U) | (1U << 2);
}

uint8_t I2C_init(void){
  // clock for B port pins
  RCC-> AHB1ENR |= (1U << 1);
  // Enable I2C1 on APB1
  RCC -> APB1ENR |= (1U << 21);

  // Software Reset to ensure I2C peripheral starts in a known state
  I2C1->CR1 |= (1U << 15);  // Set SWRST bit
  I2C1->CR1 &= ~(1U << 15); // Clear SWRST bit

  // Setup for PB6 as SCL and PB7 as SDA
  GPIOB -> MODER &= ~((3U << 12) | (3U << 14));
  GPIOB -> MODER |= (2U << 12) | (2U << 14);

  GPIOB -> AFR[0] &= ~(15U << 24);
  GPIOB -> AFR[0] |= (4U << 24);

  GPIOB -> AFR[0] &= ~(15U << 28);
  GPIOB -> AFR[0] |= (4U << 28);

  // set to open drain
  GPIOB -> OTYPER |= (1U << 6) | (1U << 7);

  // I2C Setup
  // Frequency bit for 16 MHz
  I2C1 -> CR2 = 16U;

  // in standard mode: 16 MHz / 2 * 1 KHz = 80
  I2C1 -> CCR = 80U;

  // TRISE = (1000ns / 62.5ns) + 1 = 17
  I2C1 -> TRISE = 17U;

  // Enable peripheral and set start bit to 1
  I2C1 -> CR1 |= 1U;

  return 0;
}

void ADXL345_StartRead(){
	// The hardware enters Controller (Master) Mode
	I2C1 -> CR1 |= (1U << 8);
	currentState = STATE_START_SENT;
}

uint8_t ADXL345_pwr(void){
  // Perform reads on the status registers to reset ADDR bit
  (void)I2C1 -> SR1;
  (void)I2C1 -> SR2;

  I2C1 -> DR = (0x2D);

  // Set delay for sending the bits
  // Makes sure data register is empty before proceeding
  while (!(I2C1-> SR1 & (1U << 7))){

  }

  // waiting for byte transfer finished state
  TimerStart();
  while (!(I2C1-> SR1 & (1U << 2))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  // Repeated start to change to receiver mode
  I2C1 -> CR1 |= (1U << 8);

  // Wait for SB flag
  while(!(I2C1->SR1 & (1U << 0))) {
	  // Wait until SB is 1
  }

  I2C1 -> DR = (0x53 << 1) | 1U;

  // check for ADDR register
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 1))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  // Set NACK to stop the sensor from sending information
  I2C1 -> CR1 &= ~(1U << 10);

  // Perform reads on the status registers to reset ADDR bit
  (void)I2C1 -> SR1;
  (void)I2C1 -> SR2;


  I2C1 -> CR1 |= (1U << 9); // prepare stop

  // Check if the data register is not empty
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  uint8_t devpwr = I2C1 -> DR;

  // Set ACK to 1 again to return to original state
  I2C1 -> CR1 |= (1U << 10);
  return 0;
}

uint8_t ADXL345_read(){
  // Now we get to powerctl and control the ADXL345

  // Start again, since we issued a stop previously
  I2C1 -> CR1 |= (1U << 8);

  TimerStart();
  // Wait for SB flag
  while(!(I2C1->SR1 & (1U << 0))) {
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
	  // Wait until SB is 1
  }

  I2C1 -> DR = (0x53 << 1);

  // Implements 500 ms wait time before time out
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 1))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // // Error: I2C Timeout Occurred
	  }
  }

  // Perform reads on the status registers to clear ADDR bit
  (void)I2C1 -> SR1;
  (void)I2C1 -> SR2;


  I2C1 -> DR = (0x2D);

  // Set delay for sending the bits
  // Makes sure data register is empty before proceeding
  while (!(I2C1-> SR1 & (1U << 7))){

  }

  // IMMEDIATELY send what you want to write for the register
  I2C1 -> DR = (0x08);

  // Set delay for sending the bits
  // Makes sure data register is empty before proceeding
  while (!(I2C1-> SR1 & (1U << 7))){

  }

  // wait  for byte transfer finished
  TimerStart();
  while (!(I2C1 -> SR1 & (1U << 2))){
	  if(SysTick -> CTRL & (1U << 16)){
		  return 1;
	  }
  }

  I2C1 -> CR1 |= (1U << 9); // prepare stop

  // Start reading the the ADXL345 data registers

  I2C1 -> CR1 |= (1U << 8);

  TimerStart();
  // Wait for SB flag
  while(!(I2C1->SR1 & (1U << 0))) {
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
	  // Wait until SB is 1
  }

  // write the address to SDA bus via data register
  I2C1 -> DR = (0x53 << 1);


  // Implements 500 ms wait time before time out
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 1))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  // Perform reads on the status registers to reset ADDR bit
  (void)I2C1 -> SR1;
  (void)I2C1 -> SR2;

  uint8_t data_buffer[6];

  I2C1 -> DR = (0x32);

  // Set delay for sending the bits
  // Makes sure data register is empty before proceeding
  while (!(I2C1-> SR1 & (1U << 7))){

  }

  // waiting for byte transfer finished state
  TimerStart();
  while (!(I2C1-> SR1 & (1U << 2))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  // Repeated start to change to receiver mode
  I2C1 -> CR1 |= (1U << 8);

  // Wait for SB flag
  while(!(I2C1->SR1 & (1U << 0))) {
	  // Wait until SB is 1
  }

  I2C1 -> DR = (0x53 << 1) | 1U;

  // check for ADDR register
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 1))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

  // ACK is still enabled here because we want 6 bytes in total

  // Perform reads on the status registers to reset ADDR bit
  (void)I2C1 -> SR1;
  (void)I2C1 -> SR2;

  // first byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  data_buffer[0] = I2C1 -> DR;

  // second byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  data_buffer[1] = I2C1 -> DR;

  // third byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  data_buffer[2] = I2C1 -> DR;


  // fourth byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  data_buffer[3] = I2C1 -> DR;

  // fifth byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  I2C1 -> CR1 &= ~(1U << 10); // NACK and STOP must be set before reading the second-to-last byte to terminate the sequence correctly
  I2C1 -> CR1 |= (1U << 9);
  data_buffer[4] = I2C1 -> DR;

  // sixth byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }
  data_buffer[5] = I2C1 -> DR;

  // Set ACK to 1 again to return to original state
  I2C1 -> CR1 |= (1U << 10);

  // Combine the data
  volatile int16_t x_raw = (int16_t)((data_buffer[1] << 8) | (data_buffer[0]));
  volatile int16_t y_raw = (int16_t)((data_buffer[3] << 8) | (data_buffer[2]));
  volatile int16_t z_raw = (int16_t)((data_buffer[5] << 8) | (data_buffer[4]));

  return 0;
}
