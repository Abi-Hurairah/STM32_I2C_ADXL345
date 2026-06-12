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

volatile int16_t x_raw;
volatile int16_t y_raw;
volatile int16_t z_raw;

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

  // Setup for PB8 as SCL and PB9 as SDA
  GPIOB -> MODER &= ~((3U << 16) | (3U << 18)); // Clear MODER8 and MODER9
  GPIOB -> MODER |=  ((2U << 16) | (2U << 18)); // Set both to Alternate Function (10)

  // Clear and set AFR[1] (pins 8 to 15)
  GPIOB -> AFR[1] &= ~((15U << 0) | (15U << 4)); // Clear AFSR8 and AFSR9
  GPIOB -> AFR[1] |=  ((4U << 0)  | (4U << 4));  // Set both to AF4 (0100)

  // Configure Output Type to Open Drain
  GPIOB -> OTYPER |= (1U << 8) | (1U << 9);

  // Configure Pull-ups
  GPIOB -> PUPDR &= ~((3U << 16) | (3U << 18));
  GPIOB -> PUPDR |=  ((1U << 16) | (1U << 18));

  // I2C Setup
  // Frequency bit for 16 MHz
  I2C1 -> CR2 = 16U;

  // in standard mode: 16 MHz / 2 * 1 KHz = 80
  I2C1 -> CCR = 80U;

  // TRISE = (1000ns / 62.5ns) + 1 = 17
  I2C1 -> TRISE = 17U;

  // Enable peripheral and set start bit to 1
  // The hardware enters Controller (Master) Mode
  I2C1 -> CR1 |= 1U;
  I2C1 -> CR1 |= (1U << 8);

  TimerStart();
  // Wait for SB flag
  while(!(I2C1->SR1 & (1U << 0))) {
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; //Error: I2C Timeout Occurred
	  }
	  // Wait until SB is 1
  }


  return 0;
}

uint8_t ADXL345_pwr(void){
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
  return 0;
}

uint8_t ADXL345_read(){
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

  // NACK and STOP must be set before reading the second-to-last byte to terminate the sequence correctly
  I2C1 -> CR1 &= ~(1U << 10);
  I2C1 -> CR1 |= (1U << 9);

  // fifth byte
  TimerStart();
  while(!(I2C1 -> SR1 & (1U << 6))){
	  if((SysTick -> CTRL & (1U << 16))){
		  return 1; // Error: I2C Timeout Occurred
	  }
  }

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
  x_raw = (int16_t)((data_buffer[1] << 8) | (data_buffer[0]));
  y_raw = (int16_t)((data_buffer[3] << 8) | (data_buffer[2]));
  z_raw = (int16_t)((data_buffer[5] << 8) | (data_buffer[4]));

  return 0;
}
