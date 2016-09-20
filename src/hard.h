/*
 * hard.h
 *
 *  Created on: 28/11/2013
 *      Author: Mariano
 */

#ifndef HARD_H_
#define HARD_H_

//-------- Board Configuration -----------------------//

//---- Configuracion del PWM -----//
#define WITH_DITHER
//#define WITH_PWM_DIRECT

//---- Defines for SoftStart -----//
enum {
	SOFT_INIT = 0,
	SOFT_LOW_VIN,
	SOFT_MED_VIN,
	SOFT_RUN

} typedef SoftStart;

#define VOUT_MIN	129		//10V
#define VOUT_LOW	193		//15V
#define VOUT_MED	258		//20V

//---- IO Configuration -----//
//GPIOA pin0
//GPIOA pin1
//GPIOA pin2
//GPIOA pin3
//GPIOA pin4		//Analog Inputs

//GPIOA pin5
//GPIOA pin6		//TIM3_CH1 para control PWM

//GPIOA pin7
//GPIOA pin8
//GPIOA pin9

//GPIOA pin10
#define LED ((GPIOA->ODR & 0x0400) != 0)
#define LED_ON	GPIOA->BSRR = 0x00000400
#define LED_OFF GPIOA->BSRR = 0x04000000

//GPIOA pin11
//GPIOA pin12
//GPIOA pin13
//GPIOA pin14
//GPIOA pin15

//GPIOB pin0
//GPIOB pin1
//GPIOB pin3
//GPIOB pin4
//GPIOB pin5
//GPIOB pin6
//GPIOB pin7

//---- STATES de las muestras -----//
#define FIRST_SAMPLE		0
#define SECOND_SAMPLE		1
#define THIRD_SAMPLE		2
#define FOUR_SAMPLE			3
#define WAIT_UNDERSAMPLE	4
#define UNDERSAMPLE			5




#endif /* HARD_H_ */

