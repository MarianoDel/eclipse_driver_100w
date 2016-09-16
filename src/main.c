/**
  ******************************************************************************
  * @file    Template_2/main.c
  * @author  Nahuel
  * @version V1.0
  * @date    22-August-2014
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * Use this template for new projects with stm32f0xx family.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hard.h"
#include "core_cm0.h"
#include "stm32f0x_gpio.h"
#include "stm32f0x_tim.h"
#include "stm32f0xx_it.h"
#include "adc.h"
#include "dsp.h"

//#include <stdio.h>
//#include <string.h>




//--- VARIABLES EXTERNAS ---//
volatile unsigned char timer_1seg = 0;

volatile unsigned short timer_led_comm = 0;
volatile unsigned short wait_ms_var = 0;

volatile unsigned short adc_ch[ADC_SEQ_LENGTH];

volatile unsigned char seq_ready = 0;

#define Bias_Sense		adc_ch[0]
#define Vout_Sense		adc_ch[1]
#define I_Sense			adc_ch[2]
#define Iout_Sense		adc_ch[3]
#define Vin_Sense		adc_ch[4]

//----- para los filtros ------//
//unsigned short v_pote_samples [32];
//unsigned char v_pote_index;
//unsigned int pote_sumation;


//--- VARIABLES GLOBALES ---//

// ------- de los timers -------
volatile unsigned short timer_standby;
volatile unsigned char filter_timer;
static __IO uint32_t TimingDelay;

//------- de los PID ---------
volatile int acc = 0;

//AJUSTE DE TENSION DE SALIDA VACIO
#define SP_VOUT		485			//Vout_Sense mide = Vout / 13
								//Vout = 3.3 * 13 * SP / 1024
								//400 vout 30,5V

#define KPV	128			//	4
#define KIV	16			//	64 = 0.0156 32, 16
//#define KPV	0			//	0
//#define KIV	128			//	1
#define KDV	0			// 0


#define K1V (KPV + KIV + KDV)
#define K2V (KPV + KDV + KDV)
#define K3V (KDV)

//AJUSTE DE CORRIENTE DE SALIDA
//#define MAX_I	305
#define MAX_I	340				//da 2.04A salida (tension R17 860mV)
//#define MAX_I	305				//da 1.94A salida (tension R17 1060mV)
//#define MAX_I	280				//da 1.82A salida (tension R17 860mV)
//#define MAX_I	244				//da 1.67A salida (tension R17 800mV)
//#define MAX_I	153				//da 1.25A salida (tension R17 480mV)
//#define MAX_I	75				//da 0.84A salida (tension R17 240mV)

#define KPI	16			//	0.5
#define KII	128			//	64 = 0.0156 32, 16
//#define KPV	0			//	0
//#define KIV	128			//	1
#define KDI	0			// 0

#define K1I (KPI + KII + KDI)
#define K2I (KPI + KDI + KDI)
#define K3I (KDI)

#define UNDER_FROM_44K		10		//44K / (10 * 4) = 1.1K

//todos se dividen por 128
#define KPI_DITHER	64			// 0.5
#define KII_DITHER	128			// 1
#define KDI_DITHER	4			// 0.03125

#define K1I_DITHER (KPI_DITHER + KII_DITHER + KDI_DITHER)
#define K2I_DITHER (KPI_DITHER + KDI_DITHER + KDI_DITHER)
#define K3I_DITHER (KDI_DITHER)

//AJUSTE DE CORRIENTE DE SALIDA DITHER
#define MAX_I_DITHER	340				//da 2.04A salida (tension R17 860mV)


#define DMAX	512				//maximo D permitido	Dmax = 1 - Vinmin / Vout@1024adc

#define DMAX_DITHER		DMAX

#define MAX_I_OUT		(MAX_I + 80)		//modificacion 13-07-16
#define MAX_I_MOSFET	200		//modificacion 13-07-16
								//I_Sense arriba de 620mV empieza a saturar la bobina

#define MIN_VIN			300		//modificacion 13-07-16
								//Vin_Sense debajo de 2.39V corta @22V entrada 742
								//Vin_Sense debajo de 1.09V corta @10V entrada 337

//--- FUNCIONES DEL MODULO ---//
void TimingDelay_Decrement(void);
void Update_PWM (unsigned short);
short TranslateDither (short, unsigned char);





//-------------------------------------------//
// @brief  Main program.
// @param  None
// @retval None
//------------------------------------------//
int main(void)
{
	unsigned char i;
	short error = 0;
	short d = 0;

#ifdef WITH_DITHER
	unsigned char dither_state = 0;
	short dither = 0;
#endif

	short error_z1 = 0;
	short error_z2 = 0;

	short val_k1 = 0;
	short val_k2 = 0;
	short val_k3 = 0;

	unsigned char undersampling = 0;

//	unsigned char last_main_overload  = 0;
//	unsigned char last_function;
//	unsigned char last_program, last_program_deep;
//	unsigned short last_channel;
//	unsigned short current_temp = 0;

	//!< At this stage the microcontroller clock setting is already configured,
    //   this is done through SystemInit() function which is called from startup
    //   file (startup_stm32f0xx.s) before to branch to application main.
    //   To reconfigure the default setting of SystemInit() function, refer to
    //   system_stm32f0xx.c file

	//GPIO Configuration.
	GPIO_Config();

	//ACTIVAR SYSTICK TIMER
	if (SysTick_Config(48000))
	{
		while (1)	/* Capture error */
		{
			if (LED)
				LED_OFF;
			else
				LED_ON;

			for (i = 0; i < 255; i++)
			{
				asm (	"nop \n\t"
						"nop \n\t"
						"nop \n\t" );
			}
		}
	}

	//TIM Configuration.
	TIM_3_Init();
	TIM_1_Init();

	//--- COMIENZO PROGRAMA DE PRODUCCION

	//ADC configuration.
	AdcConfig();
	ADC1->CR |= ADC_CR_ADSTART;


	//pruebo adc contra pwm
//	while (1)
//	{
//		//PROGRAMA DE PRODUCCION
//		if (seq_ready)
//		{
//			seq_ready = 0;
//			LED_ON;
//			//pote_value = MAFilter32Circular (One_Ten_Pote, v_pote_samples, p_pote, &pote_sumation);
//			pote_value = MAFilter32Pote (One_Ten_Pote);	//esto tarda 5.4us
//			//pote_value = MAFilter32 (One_Ten_Pote, v_pote_samples);	//esto tarda 32.4us
//			//pote_value = MAFilter8 (One_Ten_Pote, v_pote_samples);		//esto tarda 8.1us
//			LED_OFF;
//			Update_TIM3_CH1 (pote_value);
//		}
//	}

	Update_TIM3_CH1 (0);
	Update_TIM3_CH2 (0);
	//while (1);

	//--- Main loop ---//
	while(1)
	{
		//PROGRAMA DE PRODUCCION
		if (seq_ready)				//el sistema es siempre muestreado
		{
			//reviso el tope de corriente del mosfet
			if ((I_Sense > MAX_I_MOSFET) || (Iout_Sense > MAX_I_OUT))
			{
				//corto el ciclo
				d = 0;
			}
#ifdef WITH_DITHER
			else
			{
				//VEO SI USO LAZO V O I
				if (Vout_Sense > SP_VOUT)
				{
					//LAZO V
					error = SP_VOUT - Vout_Sense;

					acc = K1V * error;		//5500 / 32768 = 0.167 errores de hasta 6 puntos
					val_k1 = acc >> 7;

					//K2
					acc = K2V * error_z1;		//K2 = no llega pruebo con 1
					val_k2 = acc >> 7;			//si es mas grande que K1 + K3 no lo deja arrancar

					//K3
					acc = K3V * error_z2;		//K3 = 0.4
					val_k3 = acc >> 7;


					d = d + val_k1 - val_k2 + val_k3;

					//Update variables PID
					error_z2 = error_z1;
					error_z1 = error;

				}
				else
				{
					//LAZO I
					LED_ON;
					switch (dither_state)
					{
						case 0:
							//empieza o termina la secuencia de dither
							if (undersampling)
							{
								undersampling--;
							}
							else
							{
								undersampling = UNDER_FROM_44K;		//serian 10 * 4 = 40

								error = MAX_I_DITHER - Iout_Sense;	//340 es 1V en adc

								acc = K1I_DITHER * error;		//5500 / 32768 = 0.167 errores de hasta 6 puntos
								val_k1 = acc >> 7;

								//K2
								acc = K2I_DITHER * error_z1;		//K2 = no llega pruebo con 1
								val_k2 = acc >> 7;			//si es mas grande que K1 + K3 no lo deja arrancar

								//K3
								acc = K3I_DITHER * error_z2;		//K3 = 0.4
								val_k3 = acc >> 7;

								dither = dither + val_k1 - val_k2 + val_k3;
								if (dither < 0)
									dither = 0;
								else if (dither > DMAX_DITHER)		//no me preocupo si estoy con folding
									dither = DMAX_DITHER;		//porque d deberia ser chico

								//Update variables PID
								error_z2 = error_z1;
								error_z1 = error;
							}

							d = TranslateDither (dither, 0);
							dither_state++;
							break;

						case 1:
							d = TranslateDither (dither, 1);
							dither_state++;
							break;

						case 2:
							d = TranslateDither (dither, 2);
							dither_state++;
							break;

						case 3:
							d = TranslateDither (dither, 3);
							dither_state = 0;
							break;

						default:
							dither_state = 0;
							break;

					}	//fin switch dither
				}	//fin lazo V o I

				if (d < 0)
					d = 0;
				else if (d > DMAX)		//no me preocupo si estoy con folding
					d = DMAX;		//porque d deberia ser chico

			}	//fin else I_MAX
#endif
#ifdef WITH_PWM_DIRECT
			else
			{
				LED_ON;
				undersampling--;	//ojo cuando arranca resuelve 255 para abajo
				if (!undersampling)
				{
					undersampling = 5;

					//VEO SI USO LAZO V O I
					if (Vout_Sense > SP_VOUT)
					{
						//LAZO V
						error = SP_VOUT - Vout_Sense;

						acc = K1V * error;		//5500 / 32768 = 0.167 errores de hasta 6 puntos
						val_k1 = acc >> 7;

						//K2
						acc = K2V * error_z1;		//K2 = no llega pruebo con 1
						val_k2 = acc >> 7;			//si es mas grande que K1 + K3 no lo deja arrancar

						//K3
						acc = K3V * error_z2;		//K3 = 0.4
						val_k3 = acc >> 7;


						d = d + val_k1 - val_k2 + val_k3;
						if (d < 0)
							d = 0;
						else if (d > DMAX)		//no me preocupo si estoy con folding
							d = DMAX;		//porque d deberia ser chico

						//Update variables PID
						error_z2 = error_z1;
						error_z1 = error;

					}
					else
					{
						//LAZO I
						error = MAX_I - Iout_Sense;	//340 es 1V en adc

						acc = K1I * error;		//5500 / 32768 = 0.167 errores de hasta 6 puntos
						val_k1 = acc >> 7;

						//K2
						acc = K2I * error_z1;		//K2 = no llega pruebo con 1
						val_k2 = acc >> 7;			//si es mas grande que K1 + K3 no lo deja arrancar

						//K3
						acc = K3I * error_z2;		//K3 = 0.4
						val_k3 = acc >> 7;


						d = d + val_k1 - val_k2 + val_k3;
						if (d < 0)
							d = 0;
						else if (d > DMAX)		//no me preocupo si estoy con folding
							d = DMAX;		//porque d deberia ser chico

						//Update variables PID
						error_z2 = error_z1;
						error_z1 = error;

					}	//fin else lazo I
				}	//fin else undersampling
			}	//fin else I_MAX
#endif

			//pote_value = MAFilter8 (One_Ten_Pote, v_pote_samples);
			Update_TIM3_CH1 (d);

			//Update_TIM3_CH2 (Iout_Sense);	//muestro en pata PA7 el sensado de Iout

			//pote_value = MAFilter32Circular (One_Ten_Pote, v_pote_samples, p_pote, &pote_sumation);
			//pote_value = MAFilter32 (One_Ten_Pote, v_pote_samples);
			//pote_value = MAFilter8 (One_Ten_Pote, v_pote_samples);
			//pote_value = MAFilter32Pote (One_Ten_Pote);
			seq_ready = 0;
			LED_OFF;
		}	//end of seq_ready

	}	//termina while(1)

	return 0;
}


//--- End of Main ---//

unsigned char first_pattern [4] = {0x01, 0x01, 0x01, 0x01};
unsigned char second_pattern [4] = {0x01, 0x01, 0x01, 0x00};
unsigned char third_pattern [4] = {0x01, 0x00, 0x01, 0x00};
unsigned char fourth_pattern [4] = {0x00, 0x00, 0x00, 0x01};

//recibe un numero de 12 bits y un estado de 4 posibilidades
//devuelve un numero de 10bits
//con los ultimos 2 bits elijo el patron y el state es el que lo recorre
short TranslateDither (short duty_dither, unsigned char state)
{
	unsigned short duty = 0;
	unsigned short dith = 0;

	dith = duty_dither & 0x0003;

	if (dith == 0x0003)
	{
		duty = first_pattern[state];
	}
	else if (dith == 0x0002)
	{
		duty = second_pattern[state];
	}
	else if (dith == 0x0001)
	{
		duty = third_pattern[state];
	}
	else
	{
		duty = fourth_pattern[state];
	}

	duty += duty_dither >> 2;

	return duty;
}

void TimingDelay_Decrement(void)
{
	if (TimingDelay != 0x00)
	{
		TimingDelay--;
	}

	if (wait_ms_var)
		wait_ms_var--;

	if (timer_standby)
		timer_standby--;

	if (filter_timer)
		filter_timer--;

	/*
	//cuenta 1 segundo
	if (button_timer_internal)
		button_timer_internal--;
	else
	{
		if (button_timer)
		{
			button_timer--;
			button_timer_internal = 1000;
		}
	}
	*/
}
