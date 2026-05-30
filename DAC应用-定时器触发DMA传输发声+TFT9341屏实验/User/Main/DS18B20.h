

#include "stm32f10x_conf.h"
#include "hw_config.h"

/*IO²Ù×÷º¯Êý*/											   
#define	DS18B20_DQ_OUT_HIGH()     GPIO_SetBits(GPIOB, GPIO_Pin_10) 
#define	DS18B20_DQ_OUT_LOW()      GPIO_ResetBits(GPIOB, GPIO_Pin_10) 
 
#define	DS18B20_DQ_IN()             GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) 

uint8_t DS18B20_Init(void);
short DS18B20_Get_Temp(void);  
