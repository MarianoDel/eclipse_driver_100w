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
unsigned short v_pote_samples [32];
unsigned char v_pote_index;
unsigned int pote_sumation;


//--- VARIABLES GLOBALES ---//

// ------- de los timers -------
volatile unsigned short timer_standby;
volatile unsigned char filter_timer;
static __IO uint32_t TimingDelay;

volatile unsigned char door_filter;
volatile unsigned char take_sample;
volatile unsigned char move_relay;

volatile unsigned char secs = 0;
volatile unsigned short minutes = 0;

//------- de los PID ---------
volatile int acc = 0;

//AJUSTE DE TENSION DE SALIDA VACIO
#define SP_VOUT		400			//Vout_Sense mide = Vout / 13
								//Vout = 3.3 * 13 * SP / 1024

#define KPV	128			//	4
#define KIV	16			//	64 = 0.0156 32, 16
//#define KPV	0			//	0
//#define KIV	128			//	1
#define KDV	0			// 0


#define K1V (KPV + KIV + KDV)
#define K2V (KPV + KDV + KDV)
#define K3V (KDV)

//AJUSTE DE CORRIENTE DE SALIDA
#define MAX_I	305
//#define MAX_I	153				//cuando uso Iot_Sense / 2
//								//Iout_Sense mide = Iout * 0.33
//								//Iout = 3.3 * MAX_I / (0.33 * 1024)

#define KPI	128			//	4
#define KII	16			//	64 = 0.0156 32, 16
//#define KPV	0			//	0
//#define KIV	128			//	1
#define KDI	0			// 0


#define K1I (KPI + KII + KDI)
#define K2I (KPI + KDI + KDI)
#define K3I (KDI)




#define DMAX	300				//maximo D permitido	Dmax = 1 - Vinmin / Vout@1024adc

#define MAX_I_MOSFET	193		//modificacion 13-07-16
								//I_Sense arriba de 620mV empieza a saturar la bobina

#define MIN_VIN			300		//modificacion 13-07-16
								//Vin_Sense debajo de 2.39V corta @22V entrada 742
								//Vin_Sense debajo de 1.09V corta @10V entrada 337

//--- FUNCIONES DEL MODULO ---//
void TimingDelay_Decrement(void);
void Update_PWM (unsigned short);





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
	unsigned char seq_state = FIRST_SAMPLE;
	unsigned char undersample = 0;
	short d_calc = 0;
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
	Update_TIM3_CH2 (512);
	while (1);

	//--- Main loop ---//
	while(1)
	{
		//PROGRAMA DE PRODUCCION
		if (seq_ready)				//el sistema es siempre muestreado
		{
			//reviso el tope de corriente del mosfet
			if (I_Sense > MAX_I_MOSFET)
			{
				//corto el ciclo
				d = 0;
			}
#ifdef WITH_DITHER
			else
			{
				d = d_calc;
				switch (seq_state)
				{
					case FIRST_SAMPLE:		//agrego dither en cada ciclo
						undersample++;
						if (d & 0x0010)
							d += 4;

						d >>= 2;
						break;

					case SECOND_SAMPLE:
						undersample++;
						if (d & 0x0010)
							d += 4;

						d >>= 2;
						break;

					case THIRD_SAMPLE:
						undersample++;
						if (d & 0x0001)
							d += 4;

						d >>= 2;
						break;

					case FOUR_SAMPLE:
						undersample++;
						if (d & 0x0001)
							d += 4;

						d >>= 2;
						break;

					case UNDERSAMPLE:
						undersample++;
						if (undersample > 10)
						{
							//VEO SI USO LAZO V O I
							if (Vout_Sense > SP_VOUT)
							{
								//LAZO V
								error = SP_VOUT - Vout_Sense;

								//proporcional
								//val_p = KPNUM * error;
								//val_p = val_p / KPDEN;
								acc = 8192 * error;
								val_p = acc >> 15;

								//derivativo
								//val_d = KDNUM * error;
								//val_d = val_d / KDDEN;
								//val_dz = val_d;
								acc = 65536 * error;
								val_dz = acc >> 15;
								val_d = val_dz - val_dz1;
								val_dz1 = val_dz;

								d = d + val_p + val_d;
								if (d < 0)
									d = 0;
								else if (d > DMAX)
									d = DMAX;
							}
							else
							{
								//LAZO I
								undersampling--;
								if (!undersampling)
								{
									undersampling = 10;		//funciona bien pero con saltos

									//
									//							//con control por pote
									//	//						medida = MAX_I * One_Ten_Pote;	//sin filtro
									//							medida = MAX_I * pote_value;		//con filtro
									//							medida >>= 10;
									//	//						if (medida < 26)
									//	//							medida = 26;
									//	//						Iout_Sense >>= 1;
									//							error = medida - Iout_Sense;	//340 es 1V en adc
									error = MAX_I - Iout_Sense;	//340 es 1V en adc

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
							}
						}
						break;

					default:
						seq_state = FIRST_SAMPLE;
						break;
				}	//fin switch samples
			}	//fin else I_MAX
#endif
#ifdef WITH_PWM_DIRECT
			else
			{
				LED_ON;
				undersampling--;	//ojo cuando arranca resuelve 255 para abajo
				if (!undersampling)
				{
					undersampling = 10;

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

	if (take_sample)
		take_sample--;

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





