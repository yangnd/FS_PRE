#ifndef __KEY_H
#define __KEY_H	 
#include "sys.h"
/********************************************************************************	 
 * All rights reserved
********************************************************************************/   	 

#define KEY_L  		GPIO_Pin_11
#define KEY_R  		GPIO_Pin_15
#define KEY_S1   	GPIO_Pin_10
#define KEY_S2   	GPIO_Pin_8

#define READ_KEY_L()  	GPIO_ReadInputDataBit(GPIOB,KEY_L)	//��ȡ�󰴼�
#define READ_KEY_R() 	GPIO_ReadInputDataBit(GPIOC,KEY_R)	//��ȡ�Ұ���
#define READ_KEY_J1()  	GPIO_ReadInputDataBit(GPIOB,KEY_S1)	//��ȡҡ��1����
#define READ_KEY_J2()  	GPIO_ReadInputDataBit(GPIOA,KEY_S2)	//��ȡҡ��2����

//IO��ʼ��
void keyInit(void);

 //����ɨ�躯��		
void KEY_Scan(void);

#endif



