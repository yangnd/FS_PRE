#include "task_changerail.h"
#include "task_ctrldata.h"
#include "task_key.h"
#include "rs485.h"
/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static u8 rs485txbuf[8]; 
static u8 rs485rxbuf[8];
static u16 rx_crc,cal_crc;
static u8 rxlen;
static u8 uRail;
static u8 railState;
static u8 keyState;
static xSemaphoreHandle railrxIT;

/*RS485外部中断回调函数*/
static void rs485_interruptCallback(void)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(railrxIT, &xHigherPriorityTaskWoken);
//	portYIELD_FROM_ISR(xHigherPriorityTaskWoken); //如果需要的话进行一次任务切换
}
void RailCb_Init(void)
{
	railrxIT = xSemaphoreCreateBinary();
	Rail_setIterruptCallback(rs485_interruptCallback);
}
void vChangeRailTask(void *param)
{
	portBASE_TYPE state;
	static u8 step=0;
	static u8 preach=0;
	static u8 change_en=1;
	static u8 SCount=0;
	static u8 modbusCount=0;
	
	while(1)
	{
		vTaskDelay(50);
		
		keyState=getKeyState();
		if(keyState==KEY0_SHORT_PRESS)	railState=LEFTRAIL;			//置轮子处于轨道状态
		else if(keyState==KEY1_SHORT_PRESS)	railState=RIGHTRAIL;
		
		uRail=getRail();
		preach=0;
		SCount=0;
		while((uRail==1)&&(railState==RIGHTRAIL)&&change_en)//正转270度
		{
			switch(step)
			{
				case 0://选择内部位置
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x47;		//Pn071
					rs485txbuf[4]=0x7F;		//内部位置0
					rs485txbuf[5]=0xFF;		//低位
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 1://使能伺服电机
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x46;	//Pn070
					rs485txbuf[4]=0x7F;
					rs485txbuf[5]=0xFE;	//Son使能驱动器
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 2://触发内部位置，电机转动
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x47;	//Pn071
					rs485txbuf[4]=0x7B;	//内部位置0,触发
					rs485txbuf[5]=0xFF;
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
							vTaskDelay(500);							
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 3://读取Dn018，判断bit3-Preach
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x03;	//读
					rs485txbuf[2]=0x01;
					rs485txbuf[3]=0x82;	//Dn018
					rs485txbuf[4]=0x00;	//读1个寄存器
					rs485txbuf[5]=0x01;
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						SCount++;
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							if(RS485_RX_BUF[4]&0x08)//取Bit3 Preach,Bit 位为 0，表示功能为 ON 状态，为 1 则是 OFF 状态
							{
								//位置偏差，发出偏差报警
								preach=0;
							}
							else
							{
								//到达指定位置，进行下一步
								preach=1;
								step++;
								modbusCount=0;
							}
							
						}
						else
						{
							//校验错误
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 4://失能伺服电机
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x46;	//Pn070
					rs485txbuf[4]=0x7F;
					rs485txbuf[5]=0xFF;	//Son失能驱动器
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//变轨指令完成
							if(preach)
							{
								railState=LEFTRAIL;
								preach=0;//清除状态
							}
							else
							{
								change_en=0;//指令完成但位置偏差时，禁止变轨
							}
							step=0;//复位步骤
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
			}		
		}
		while((uRail==2)&&(railState==LEFTRAIL)&&change_en)//反转270度
		{
			switch(step)
			{
				case 0://选择内部位置
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x47;		//Pn071
					rs485txbuf[4]=0x7E;		//内部位置1
					rs485txbuf[5]=0xFF;		//低位
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 1://使能伺服电机
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x46;	//Pn070
					rs485txbuf[4]=0x7F;
					rs485txbuf[5]=0xFE;	//Son使能驱动器
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 2://触发内部位置，电机转动
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x47;	//Pn071
					rs485txbuf[4]=0x7A;	//内部位置1,触发
					rs485txbuf[5]=0xFF;
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//写成功，进行下一步
							step++;
							modbusCount=0;
							vTaskDelay(500);
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 3://读取Dn018，判断bit3-Preach
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x03;	//读
					rs485txbuf[2]=0x01;
					rs485txbuf[3]=0x82;	//Dn018
					rs485txbuf[4]=0x00;	//读1个寄存器
					rs485txbuf[5]=0x01;
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						SCount++;
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							if(RS485_RX_BUF[4]&0x08)//取Bit3 Preach,Bit 位为 0，表示功能为 ON 状态，为 1 则是 OFF 状态
							{
								//位置偏差，发出偏差报警
								preach=0;
							}
							else
							{
								//到达指定位置，进行下一步
								preach=1;
								step++;
								modbusCount=0;
							}
							
						}
						else
						{
							//校验错误
						}
					}
					else
					{
						modbusCount++;
					}
					break;
				case 4://失能伺服电机
					rs485txbuf[0]=0x02;
					rs485txbuf[1]=0x06;
					rs485txbuf[2]=0x00;
					rs485txbuf[3]=0x46;	//Pn070
					rs485txbuf[4]=0x7F;
					rs485txbuf[5]=0xFF;	//Son失能驱动器
					ModbusWriteSReg(rs485txbuf,8);
					state=xSemaphoreTake(railrxIT, MODBUS_TIME);
					if(state==pdTRUE)
					{
						cal_crc=crc_chk(RS485_RX_BUF,RS485_RX_CNT-2);
						rx_crc=((u16)RS485_RX_BUF[RS485_RX_CNT-1]<<8)+RS485_RX_BUF[RS485_RX_CNT-2];
						if(cal_crc==rx_crc)
						{
							//变轨指令完成
							if(preach)
							{
								railState=RIGHTRAIL;
								preach=0;//清除状态
							}
							else
							{
								change_en=0;//指令完成但位置偏差时，禁止变轨
							}
							step=0;//复位步骤
							modbusCount=0;
						}
						else
						{
							//校验错误，返回执行本步骤
						}
					}
					else
					{
						modbusCount++;
					}
					break;
			}		
		}
	}
}
