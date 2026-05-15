#include "utils_printf.h"
unsigned char BitSend[9]={0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

#if 0
void Gpio_Print_Init(void){
	GPIO_InitTypeDef	GPIO_InitStructure;        
	GPIO_InitStructure.Pin  = GPIO_Pin_1;
	GPIO_InitStructure.Mode = GPIO_OUT_PP;         
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);
}
#endif
#if DEBUG_SOFT_EN == TRUE
void Gpio_Print_HL(uint8_t hl){
    P31 = hl;
}
void Delay8us(void)	//@35MHz
{
	unsigned char data i;
//	i = 90;
    i = 73;
	while (--i);
}
void Soft_Put_Char(char c){
    char i = 0;
	Gpio_Print_HL(FALSE);
    Delay8us();
    for(i = 1; i < 9; i++)
    {
        if(BitSend[i] & c)
            Gpio_Print_HL(TRUE);
        else
            Gpio_Print_HL(FALSE);
        
        Delay8us();
    }
    Gpio_Print_HL(TRUE);
    Delay8us();
}
#endif
char putchar(char c)
{
    #if DEBUG_SWITCH == TRUE
    Soft_Put_Char(c);
    #endif
	return c;
}

#if 0
void uprintf_array(uint8_t *arraydata,uint16_t len){
    
    uint16_t i=0;
    printf("\r\narray:\r\n");
    for(;i<len;i++){
        printf(" %b02x",arraydata[i]);
    }
    printf("\r\nend\r\n");
   
}
 #endif