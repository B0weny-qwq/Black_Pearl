#include "gyro.h"
#include "KalmanFiltering.h"
#include "utils_printf.h"
uint8_t i2c_addr = 0x27;
int16_t offset_x=0, offset_y=0, offset_z=0;

uint8_t gyro_Y_Datas[20] = {0};
//Fill in the platform read/write function
#define SLAW    0x4e
#define SLAR    0x4f


int8_t mir3da_register_read(uint8_t addr, uint8_t *data_m, uint8_t len)
{
    SI2C_ReadNbyte(SLAW,addr,data_m,len);
    // MyI2C_RecvBuffer(SLAW,addr,data_m,len);
    return 0;
}

int8_t mir3da_register_write(uint8_t addr, uint8_t data_m)
{   
    SI2C_WriteNbyte(SLAW,addr,(uint8_t *)&data_m,1);
    // MyI2C_SendBuffer(SLAW,addr,(uint8_t *)&data_m,1);
    // MyI2C_Send_Byte(SLAW,addr,data_m);
    return 0;
}
#if 0
int8_t mir3da_register_mask_write(uint8_t addr, uint8_t mask, uint8_t data_m)
{
    int res=0;
    unsigned char tmp_data=0;
	
    res = mir3da_register_read(addr, &tmp_data, 1);
    tmp_data &= ~mask; 
    tmp_data |= data_m & mask;
    res = mir3da_register_write(addr, tmp_data);
  
    return 0;
}
#endif

#if 1
//mir3da_init -> mir3da_set_enable -> mir3da_read_raw_data
//Initialization
int8_t mir3da_init(void){
	uint8_t data_m = 0;
	int i;

	for(i=0; i<10; i++){
        delay_ms(100);
		mir3da_register_read(REG_CHIP_ID, &data_m, 1);
        if(data_m != 0x13){
            printf("REG_CHIP_ID read False=%bd\r\n",data_m);			
        }else{
            break;
        }
	}
    printf("REG_CHIP_ID=%b02x\r\n",data_m);
    mir3da_register_write(REG_SPI_CONFIG, (0x81 | 0x24));
    delay_ms(20);


    mir3da_register_write(REG_RESOLUTION_RANGE, 0x04);  //0x04; +/-2G,12bit
    mir3da_register_write(REG_ODR_AXIS, 0x99);          //PERF LEVEL1 100hz
    mir3da_register_write(REG_MODE_AXIS, 0x00);         //normal mode, enableXYZ, autosleep disable
   

    printf("REG_ACTIVE_STATUS read1 =%b02x\r\n",data_m);
    return 0;
}
//Read three axis data, 2g range, counting 12bit, 1G=1000mg=1024lsb
void mir3da_read_raw_data(short *x, short *y, short *z)
{

    uint8_t tmp_data[6] = {0};
    mir3da_register_read(REG_ACC_X_LSB, (uint8_t *)&tmp_data, 6);

    uprintf_array(&tmp_data,6);
    
    *x = ((short)(tmp_data[1] << 8 | tmp_data[0]))>> 4;
    *y = ((short)(tmp_data[3] << 8 | tmp_data[2]))>> 4;
    *z = ((short)(tmp_data[5] << 8 | tmp_data[4]))>> 4;
    printf("x==%b04x,y=%b04x,z=%b04x\r\n",*x,*y,*z );
}
#endif
void mir3da_read_data(short *x, short *y, short *z)
{
    uint8_t tmp_data[6] = {0};
   
	mir3da_register_read(REG_ACC_X_LSB, tmp_data, 6);
    //uprintf_array(&tmp_data,6);
    *x = (((short)(tmp_data[1] << 8 | tmp_data[0])) >> 4) - offset_x;
    *y = (((short)(tmp_data[3] << 8 | tmp_data[2])) >> 4) - offset_y;
    *z = (((short)(tmp_data[5] << 8 | tmp_data[4])) >> 4) - offset_z;
    //printf("x==%bd,y=%bd,z=%bdx\r\n",*x,*y,*z );

}
/*
  The chip needs to keep horizontal.
*/
void do_difff_calibrate(void)
{
	int i;
    short x=0, y=0, z=0;
    int16_t off_x=0, off_y=0, off_z=0;
    int32_t totalx = 0,totaly = 0,totalz =0;
	
 	//must power on
    for(i=0; i<20; i++)
    {
        mir3da_read_raw_data(&x, &y, &z);
        totalx += (int32_t)x;
        totaly += (int32_t)y;
        totalz += (int32_t)z;
		delay_ms(20);
    }
    
    off_x = (int16_t)(totalx/20 - 0);
    off_y = (int16_t)(totaly/20 - 0);
    if(totalz>0)
        off_z = (int16_t)(totalz/20 - SENSITIVITY);
    else
        off_z = (int16_t)(totalz/20 + SENSITIVITY);

    offset_x = off_x;
    offset_y = off_y;
    offset_z = off_z;
    printf("offset x==%b02x,y=%b02x,z=%b02x\r\n",offset_x,offset_y,offset_z);
}

void gs_get_tilt(short *XYZangle,short *xyzdata)
{ 
    short gx,gy,gz;
    short *xAngle,*yAngle,*zAngle;
    short  yz_plane, xz_plane , xy_plane; 
    short tanX,tanY,tanZ;

    xAngle = XYZangle;
    yAngle = XYZangle+1;
    zAngle = XYZangle+2;
    
    gx = (short)(*xyzdata);
    gy = (short)(*(xyzdata+1));
    gz = (short)(*(xyzdata+2));
  
    yz_plane = (short)(sqrt((gy)*(gy)+(gz)*(gz)));
    xz_plane = (short)(sqrt((gx)*(gx)+(gz)*(gz)));
    xy_plane = (short)(sqrt((gx)*(gx)+(gy)*(gy)));

    if(yz_plane==0.0)
    {
        *xAngle = 90.0;
    }
    else
    {
        tanX = ((short)(gx))/yz_plane;
        *xAngle = atan(tanX)*180.0/3.14;
    }
    
       
    if(xz_plane==0.0)
    {
        *yAngle = 90.0; 
    }
    else
    {
        tanY = ((short)(gy))/xz_plane; 
        *yAngle = atan(tanY)*180.0/3.14;
    }
    
    if((gz) == 0.0)
    {
        *zAngle = 90.0;  
    }
    else
    {
        tanZ = xy_plane/((short)(gz));  
        *zAngle = atan(tanZ)*180.0/3.14; 
    }  
}

void Gyro_Read_Handle(void){
    static uint8_t gryo_delay_Times = 0;
    static short gryo_x=0,gryo_y=0,gryo_z=0;
    static short last_gryo_y=0;
    gryo_delay_Times++;
    if(gryo_delay_Times<20){
        gryo_delay_Times++;
        return;
    }else{
        gryo_delay_Times = 0;
    }

    // mir3da_read_raw_data((short *)&gryo_x,(short *)&gryo_y,(short *)&gryo_z);
    mir3da_read_data((short *)&gryo_x,(short *)&gryo_y,(short *)&gryo_z);
    
    last_gryo_y = gryo_y;
}


