/******************************************************************
**	  NUAA_CM3_107实验开发板（V2.0）
**	  读取18b20温度值，并在3.2吋屏上显示
**	  作    者：NUAA
**	  完成日期:	2016.8.25
********************************************************************/
#include "DS18B20.h"
#include "delay.h"

/**************************************************
*函数名称：void DS18B20_Rst(void)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：复位DS18B20	(每次读写均要先复位)
***************************************************/
void DS18B20_Rst(void)	   
{                 
	DS18B20_DQ_OUT_LOW();	     // 拉低DQ 
    Delay_us(750);               // 延时750us 
	DS18B20_DQ_OUT_HIGH();       // 拉高DQ	
	Delay_us(15);                // 延时15US  
}


/**************************************************
*函数名称：uint8_t DS18B20_Init(void)
*
*入口参数：无
*
*出口参数：1，未检到DS18B20的存在 0，存在
*
*功能说明：等待DS18B20的回应
***************************************************/
u8 DS18B20_Check(void) 	   
{   
	u8 retry=0;
    while (DS18B20_DQ_IN()&&retry<200)
	{
		retry++;
		Delay_us(1);
	};	 
	if(retry>=200)return 1;
	else retry=0;
    while (!DS18B20_DQ_IN()&&retry<240)
	{
		retry++;
		Delay_us(1);
	};
	if(retry>=240)return 1;	    
	return 0;
}


/**************************************************
*函数名称：uint8_t DS18B20_Init(void)
*
*入口参数：无
*
*出口参数：1，未检到DS18B20的存在 0，存在
*
*功能说明：初始化18b20
***************************************************/
uint8_t DS18B20_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  //管脚频率50MHz
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  //开漏输出 
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	DS18B20_Rst();		                              //复位 
	return DS18B20_Check();							  //0 存在; 1 未检测到DS18B20的存在 
}
/**************************************************
*函数名称：void DS18B20_Write_Byte(u8 dat)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：写一个字节
***************************************************/
void DS18B20_Write_Byte(u8 dat)     
 {             
    u8 j;
    u8 testb;
    for (j=1;j<=8;j++) 
	{
        testb=dat&0x01;
        dat=dat>>1;
        if (testb) 
        {
			DS18B20_DQ_OUT_LOW();
            Delay_us(2);                            
			DS18B20_DQ_OUT_HIGH();
            Delay_us(60);             
        }
        else 
        {
			DS18B20_DQ_OUT_LOW();
            Delay_us(60);             
			DS18B20_DQ_OUT_HIGH();
            Delay_us(2);                          
        }
    }
}


/**************************************************
*函数名称：void DS18B20_Start(void)
*
*入口参数：无
*
*出口参数：无
*
*功能说明：开始温度转换
***************************************************/
void DS18B20_Start(void)
{   						               
    DS18B20_Rst();	   				//复位DS18B20
	DS18B20_Check();	 			//等待DS18B20的回应
	//Delay_ms(500); 
    DS18B20_Write_Byte(0xcc);       //跳过ROM
    DS18B20_Write_Byte(0x44);       //发送开始转换命令
} 


/**************************************************
*函数名称：u8 DS18B20_Read_Bit(void)
*
*入口参数：无
*
*出口参数：1/0
*
*功能说明：从DS18B20读取一个位
***************************************************/
u8 DS18B20_Read_Bit(void) 			
{
    u8 data;
    DS18B20_DQ_OUT_LOW(); 
	Delay_us(2);
    DS18B20_DQ_OUT_HIGH(); 
	Delay_us(12);
	if(DS18B20_DQ_IN())data=1;
    else data=0;	 
    Delay_us(50);           
    return data;
}


/**************************************************
*函数名称：u8 DS18B20_Read_Bit(void)
*
*入口参数：无
*
*出口参数：读取的数据dat
*
*功能说明：从DS18B20读取一个字节
***************************************************/
u8 DS18B20_Read_Byte(void)    
{        
    u8 i,j,dat;
    dat=0;
	for (i=1;i<=8;i++) 
	{
        j=DS18B20_Read_Bit();
        dat=(j<<7)|(dat>>1);
    }						    
    return dat;
}
																											

/**************************************************
*函数名称：short DS18B20_Get_Temp(void)
*
*入口参数：无
*
*出口参数：温度值（-550~1250） 
*
*功能说明：从ds18b20获取温度值 精度0.1C
***************************************************/
short DS18B20_Get_Temp(void)
{
    u8 temp;
    u8 TL,TH;
	short tem;
    DS18B20_Start ();                    // 开始转换
    DS18B20_Rst();						 //复位18b20
    DS18B20_Check();	 				 //检测18b20是否存在
    DS18B20_Write_Byte(0xcc);            //跳过ROM
    DS18B20_Write_Byte(0xbe);	 //读寄存器，共九字节，前两字节为转换值 
	
    TL=DS18B20_Read_Byte();               // LSB   
    TH=DS18B20_Read_Byte();               // MSB  
	    	  
    if(TH>7)
    {
        TH=~TH;
        TL=~TL; 
        temp=0;                          //温度为负  
    }else temp=1;                        //温度为正	  	  
    tem=TH;                              //获得高八位
    tem<<=8;    
    tem+=TL;                            //获得低八位
    tem=(float)tem*0.625;               //转换     
	if(temp)return tem;                 //返回温度值
	else return -tem;    
} 

