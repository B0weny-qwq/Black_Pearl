/*---------------------------------------------------------------------*/
/* --- STC MCU Limited ------------------------------------------------*/
/* --- STC 1T Series MCU Demo Programme -------------------------------*/
/* --- Mobile: (86)13922805190 ----------------------------------------*/
/* --- Fax: 86-0513-55012956,55012947,55012969 ------------------------*/
/* --- Tel: 86-0513-55012928,55012929,55012966 ------------------------*/
/* --- Web: www.STCAI.com ---------------------------------------------*/
/* --- BBS: www.STCAIMCU.com  -----------------------------------------*/
/* --- QQ:  800003751 -------------------------------------------------*/
/* 锟斤拷锟斤拷?锟节筹拷锟斤拷锟斤拷使锟矫此达拷锟斤拷,锟斤拷锟节筹拷锟斤拷锟斤拷注锟斤拷使锟斤拷锟斤拷STC锟斤拷锟斤拷锟较硷拷锟斤拷锟斤拷            */
/*---------------------------------------------------------------------*/

#ifndef		__TYPE_DEF_H
#define		__TYPE_DEF_H

//========================================================================
//                               锟斤拷锟酵讹拷锟斤拷
//========================================================================

typedef unsigned char   u8;     //  8 bits 
typedef unsigned int    u16;    // 16 bits 
typedef unsigned long   u32;    // 32 bits 

typedef signed char     int8;   //  8 bits 
typedef signed int      int16;  // 16 bits 
typedef signed long     int32;  // 32 bits 

typedef unsigned char   uint8;  //  8 bits 
typedef unsigned int    uint16; // 16 bits 
typedef unsigned long   uint32; // 32 bits 


typedef signed char int8_t;
typedef unsigned char   uint8_t;
typedef short  int16_t;
typedef unsigned short  uint16_t;
typedef long  int32_t;
typedef unsigned long uint32_t;


//===================================================

#define	TRUE	1
#define	FALSE	0

//===================================================

#define	NULL	0

//===================================================

#define	Priority_0			0	//中断优先级为 0 级（最低级）
#define	Priority_1			1	//中断优先级为 1 级（较低级）
#define	Priority_2			2	//中断优先级为 2 级（较高级）
#define	Priority_3			3	//中断优先级为 3 级（最高级）

#define ENABLE		1
#define DISABLE		0

#define SUCCESS		0
#define FAIL		-1

//===================================================

#define	I2C_Mode_Master			1
#define	I2C_Mode_Slave			0

#define	PIE			0x20	//1: 比较结果由0变1, 产生上升沿中断
#define	NIE			0x10	//1: 比较结果由1变0, 产生下降沿中断

#define	PWMA	128
#define	PWMB	129

//===================================================

#endif
