/************************************************
*  各种波波算法实现
*************************************************/

#include "stdlib.h"
#define A 10       //A值可根据实际情况调整
#include "stm32f10x_conf.h"
#include "hw_config.h"

/**********************中位值滤波****************/
//#define N 11       //N值可根据实际情况调整
#define N 12
#define a   0.25
u16 value;        //value为已有值
u16 value_buf[N];
u8 i=0;
int sum=0;
u8 const jq[N]={1,2,3,4,5,6,7,8,9,10,11,12};//加权系数表
u8 const sum_jq=1+2+3+4+5+6+7+8+9+10+11+12;

int filter_ZWZ(void)
{
u16 count,i,j,temp;
for (count=0;count<N;count++)
{
value_buf[count]=Read_ADC1_MultiChannel(ADC_Channel_3);   //获取采样值
//delay();
}
for (j=0;j<N-1;j++)    //采样值由小到大排列，排序采用冒泡法
{
for (i=0;i<N-j;i++)
{
if(value_buf[i]>value_buf[i+1])
{
temp=value_buf[i];
value_buf[i]=value_buf[i+1];
value_buf[i+1]=temp;
}
}
}
return value_buf[(N-1)/2];    //取中间值
}
/*****************算术平均滤波******************/
int filter_SSPJ()
{
int sum=0,count=0;
for(count=0;count<N;count++)
{
sum+=Read_ADC1_MultiChannel(ADC_Channel_3);
//delay();
}
return (u16)(sum/N);
}

/****************去极值平均滤波 ************************/
int filter_QJZQPJ()
{
u16 count,i,j,temp;
for (count=0;count<N;count++)
{
value_buf[count]=Read_ADC1_MultiChannel(ADC_Channel_3);     //获取采样值
//delay();
}
for (j=0;j<N-1;j++)   //采样值由小到大排列，排序采用冒泡法
{
for (i=0;i<N-j;i++)
{
if(value_buf[i]>value_buf[i+1])
{
temp=value_buf[i];
value_buf[i]=value_buf[i+1];
value_buf[i+1]=temp;
}
}
}
sum=0;
for(count=1;count<(N-1);count++)     /*去掉第一个和末一个数*/
{
sum+=value_buf[count];
//delay();
}
return (u16)(sum/(N-2));
}
/***************移动平均滤波（递推平均滤波）*************/
int filter_YDPJ()
{
u8 count;
int sum=0;
value_buf[i++]=Read_ADC1_MultiChannel(ADC_Channel_3);
if(i==N) i=0;
for (count=0;count<N;count++)
sum+=value_buf[count];
return (u16)(sum/N);
}

/*************加权平均滤波**********************/
int filter_JQPJ()
{
u8 count;
int sum=0;
for (count=0;count<N;count++)
{value_buf[count]=Read_ADC1_MultiChannel(ADC_Channel_3);     //获取采样值
//delay();
}
for (count=0;count<N;count++)
sum+=value_buf[count]*jq[count];
return (u16)(sum/sum_jq);
}

/**********************低通滤波**********************/
int filter_DT(void)
{
u16 new_value;    //new value为当前采样值
new_value=Read_ADC1_MultiChannel(ADC_Channel_3);
return (a*new_value+(1-a)*value);
}
