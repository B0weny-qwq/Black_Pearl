/**
 * @file    Type_def.h
 * @brief   工程基础类型与通用宏定义。
 *
 * @details
 * 本文件提供当前 STC32G 工程共用的整型别名、布尔/使能宏、优先级定义和
 * 成功失败返回值。`Config.h` 会统一引入本文件，设备层和用户层均依赖这里的
 * 类型口径。
 */
/**
 * @note 当前职责边界：
 * - 本文件只提供工程公共基础类型、布尔/使能宏和通用返回值定义。
 * - 它不承载业务策略、外设配置或模块关系逻辑。
 */
#ifndef		__TYPE_DEF_H
#define		__TYPE_DEF_H

typedef unsigned char   u8;     //  8 bits 
typedef unsigned int    u16;    // 16 bits 
typedef unsigned long   u32;    // 32 bits 
 
typedef signed char     s8;     //  8 bits
typedef signed char     int8;   //  8 bits 
typedef signed int      int16;  // 16 bits 
typedef signed long     int32;  // 32 bits 

typedef unsigned char   uint8;  //  8 bits 
typedef unsigned int    uint16; // 16 bits 
typedef unsigned long   uint32; // 32 bits 

#define	TRUE	1
#define	FALSE	0

#define	NULL	0

#define	Priority_0			0	//中断优先级为 0 级（最低级）
#define	Priority_1			1	//中断优先级为 1 级（较低级）
#define	Priority_2			2	//中断优先级为 2 级（较高级）
#define	Priority_3			3	//中断优先级为 3 级（最高级）

#define ENABLE		1
#define DISABLE		0

#define SUCCESS		0
#define FAIL		-1


#endif
