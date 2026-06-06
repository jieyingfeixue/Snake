/**************NUAA_CM3_107实验开发板*********************************
描述： Descriptions:        一些简单外设初始化设置																	 
*********************************************************************************/ 

#include "hw_config.h"
#define SampleNum 10			/*每个通道采样40次*/
#define ChannelNum 5			/*共5个通道:0＝chan3,1=chan6,2=chan7,3=chan16,4-chan17   */
#define ADC_DR_Address    ((u32)0x4001244C)  /*0x4001244C为ADC_DR的地址（规则数据寄存器地址）*/
#define DAC_DHR12RD_Address  ((u32) 0x40007420);/* DAC数据寄存器地址*/
u16 ADC_ConvertedValue[SampleNum][ChannelNum];  /*ADC转换结果存放变量（地址）*/
extern u16 value;

/*********************************************************************************
	*函数名称：void LED_Configuration(void)
	*
	*入口参数：无
	*
	*出口参数：无
	*
	*功能说明：LED初始化配置
*********************************************************************************/
void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD, ENABLE);
	GPIO_InitStructure.GPIO_Pin = LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LED5_PIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;	
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;	
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = BEEP_PIN ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;	
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/*---------初始化状态四个LED全OFF------------*/
	LED1(1);
	LED2(1);
	LED3(1);
	LED4(1);
			
}


/*********************************************************************************
	*函数名称：voidADC_Mode_config(void)
	*
	*入口参数：无
	*
	*出口参数：无
	*
	*功能说明：配置ADC1的工作模式为DMA模式 
*********************************************************************************/

 void ADC_Mode_Config(void)
{
  DMA_InitTypeDef DMA_InitStructure;
  ADC_InitTypeDef ADC_InitStructure;	
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); //使能DMA1时钟
	/* DMA channel1 configuration */
  DMA_DeInit(DMA1_Channel1);  //指定DMA通道
  DMA_InitStructure.DMA_PeripheralBaseAddr = ADC_DR_Address;//设置DMA外设地址ADC_DR_Adress
  DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_ConvertedValue;	//设置DMA内存地址，ADC转换结果直接放入该地址
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; //外设为设置为数据传输的来源
  DMA_InitStructure.DMA_BufferSize = SampleNum*ChannelNum;	//DMA缓冲区设置为SampleNum*ChannelNum；
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//DMA不允许地址递增
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;//内存地址递增
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel1, &DMA_InitStructure);
  
  /* Enable DMA channel1 */
  DMA_Cmd(DMA1_Channel1, ENABLE);  //使能DMA通道

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//使能ADC1时钟
     
  /* ADC1 configuration */
  ADC_InitStructure.ADC_Mode = ADC_Mode_Independent; //使用独立模式
  ADC_InitStructure.ADC_ScanConvMode = ENABLE;			 //扫描模式允许，多通道必须使能
  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE; //连接转换模式，无需外接触发器
  ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;//不用外部触发
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; //使用数据右对齐
  ADC_InitStructure.ADC_NbrOfChannel = ChannelNum;  // ChannelNum个通道转换通道
  ADC_Init(ADC1, &ADC_InitStructure);

  /* ADC1 regular channel11 configuration */ 
  ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_55Cycles5); //通道3（电位器）采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 2, ADC_SampleTime_55Cycles5); //通道6（PT100）采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 3, ADC_SampleTime_55Cycles5); //通道7（电流电压）采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_16, 4, ADC_SampleTime_55Cycles5); //通道16（内部温度）采样周期55.5个时钟周期
  ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 5, ADC_SampleTime_55Cycles5); //通道17（基准电压）采样周期55.5个时钟周期

  /* Enable ADC1 DMA */
  ADC_DMACmd(ADC1, ENABLE);	 //使能ADC的DMA
  
  /* Enable ADC1 */
  ADC_Cmd(ADC1, ENABLE); //使能ADC1

	ADC_TempSensorVrefintCmd(ENABLE);			 					 /*使能温度传感器和内部参考电压通道*/	

  /* Enable ADC1 reset calibaration register */   
  ADC_ResetCalibration(ADC1);
  /* Check the end of ADC1 reset calibration register */
  while(ADC_GetResetCalibrationStatus(ADC1));

  /* Start ADC1 calibaration */
  ADC_StartCalibration(ADC1);
  /* Check the end of ADC1 calibration */
  while(ADC_GetCalibrationStatus(ADC1));
     
  /* Start ADC1 Software Conversion */ 
  ADC_SoftwareStartConvCmd(ADC1, ENABLE);  //开始转换
}

extern uint32_t DAC_DHR12R2_Address ; 
extern uint32_t DAC2Sine12bit[32];

/**********************************************
*函数名称：void DAC_Configuration(void)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：DAC初始化
*********************************************/
void DAC_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	DAC_InitTypeDef DAC_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC ,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO,ENABLE);		   

	/*-----------DAC端口配置 复用输出模式-------------*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;	 //PA5:DAC2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA ,&GPIO_InitStructure);
	DAC_DeInit();									  //还原到初始状态

	/* DAC 通道2配置 */
	DAC_InitStructure.DAC_Trigger = DAC_Trigger_T3_TRGO;		       //选择定时器3作外部触发源
	DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;	   //自定义波形产生
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable;	   //失能输出缓冲
	DAC_Init(DAC_Channel_2, &DAC_InitStructure);					   //DAC初始化
	DAC_Cmd(DAC_Channel_2,  ENABLE);								   //使能DAC
	DAC_SoftwareTriggerCmd(DAC_Channel_2, ENABLE);					   //通道2由软件触发
	
}


/**********************************************
*函数名称：void TIMER_Configuration(void)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：TIM3初始化
*********************************************/
void TIM_Configuration(uint16_t f)
{

	TIM_TimeBaseInitTypeDef	TIM_TimeBaseStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 ,ENABLE);
	TIM_DeInit(TIM3);
	TIM_TimeBaseStructure.TIM_Period = SystemCoreClock/1/32/f-1;//一个周期32个点，两间隔离时间的倒数即频率，改变这个值能改变正弦波的频率 反比例变化    
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;       //这个值要为零 否则无输出   	 
	TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;   //这个值要为零 否则无输出
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	
	TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);  //使用更新事件作为触发输出
	
	TIM_Cmd(TIM3, ENABLE);	   //使能TIM3
}


/**********************************************
*函数名称：void DMA_Configuration(void)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：DMA2初始化
*********************************************/
void DMA_Configuration(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2 ,ENABLE);				 //打开DMA2时钟
	DMA_DeInit(DMA2_Channel4);								         //将DAM通道4 的寄存器设为默认值
	DMA_InitStructure.DMA_PeripheralBaseAddr = DAC_DHR12R2_Address;	 //DAC2  12位右对齐数据寄存器地址
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&DAC2Sine12bit; //待送入DAC2的数字量
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;				 //外设作为数据传输的来源
	DMA_InitStructure.DMA_BufferSize = 32;						     //DMA缓存大小，32个数据（一个周期的正弦波点数）
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; //外设寄存器地址不变
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;			 //内存地址寄存器递增
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;//外设为16位（12位DAC）
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;//内存数据宽度采用16位模式，对应DAC12位
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;			 //工作在循环缓存模式
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;		 //设置DMA通道优先级为高
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;			 //不设置为内存到内存传输

	DMA_Init(DMA2_Channel4, &DMA_InitStructure);

	DMA_Cmd(DMA2_Channel4, ENABLE);		   

	/*使能DAC通道2:PA.5被自动连接到DAC的转换器*/
	DAC_Cmd(DAC_Channel_2, ENABLE);
	
	/* Enable DMA for DAC Channel2 */
	DAC_DMACmd(DAC_Channel_2, ENABLE);				  //使能DAC通道2的DMA
}

static	uint8_t  sysclk = 72;   /*默认系统时钟为72MHz*/
static  uint8_t  fac_us=0;      /*us延时倍乘数	*/
static  uint16_t fac_ms=0;      /*ms延时倍乘数 */

/*********************************************************************************
	*名称：void Delay_Init(void)
	*参数：无
	*返回：无
	*功能：基准延时初始化，使用SysTick
	*备注：SysTick时钟源由HCLK/8提供，当系统频率为72MHz时
           最小计数周期为1/9MHz,计满9次为1us，fac_us以9为单位  
*********************************************************************************/

void Delay_Init(void)
{
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	fac_us = sysclk/8;		    
	fac_ms=(u16)fac_us*1000;
}





/*********************************************************************************
	*名称：void Delay_ms(u16 nms)
	*参数：nms(ms number) 毫秒数 不能超过1864
	*返回：无
	*功能：毫秒级延时
	*备注：SysTick->LOAD为24位寄存器,所以最大延时小于或等于1864ms，
*********************************************************************************/
void Delay_ms(u16 nms)
{	 		  	  
	u32 temp;		   
	SysTick->LOAD=(u32)nms*fac_ms;/*时间加载(SysTick->LOAD为24bit)*/
	SysTick->VAL =0x00;           /*清空计数器 */
	SysTick->CTRL=0x01 ;          /*开始倒数  */  
	do
	{
		temp=SysTick->CTRL;
	}
	while(temp&0x01&&!(temp&(1<<16))); /*等待时间到达   */
	SysTick->CTRL=0x00;       /*关闭计数器	 */
	SysTick->VAL =0X00;       /*清空计数器	*/  	    
} 


/*********************************************************************************
	*名称：void Delay_us(u32 nus)
	*参数：us(number us) 延时微秒数 最大1864135
	*返回：无
	*功能: 实现微秒级延时，最大延时1864ms
	*备注：这两个函数是通过寄存器配置的，没有对应的固件库函数，参考misc.h
*********************************************************************************/		    								   
void Delay_us(u32 nus)
{		
	u32 temp;	    	 
	SysTick->LOAD=nus*fac_us; /*时间加载 fac_us=9,走9次就是1us	*/  		 
	SysTick->VAL=0x00;        /*清空计数器 */
	SysTick->CTRL=0x01 ;      /*开始倒数 */	 
	do
	{
		temp=SysTick->CTRL;
	}
	while(temp&0x01&&!(temp&(1<<16)));/*等待时间到达*/   
	SysTick->CTRL=0x00;       /*关闭计数器	*/
	SysTick->VAL =0X00;       /*清空计数器	*/
}

/*---LCD延时函数 10MS*nCount-----*/
void Delay_10ms(uint32_t nCount)
{
	volatile int i;	 	
	for (i=0;i<nCount*100;i++);
}
void Delay(void)
{
	int i,j;
	for(i=1000;i>0;i--)
		for(j=1000;j>0;j--);

}
