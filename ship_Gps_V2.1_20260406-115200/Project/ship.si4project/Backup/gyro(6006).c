#include "gyro.h"
#include "utils_printf.h"
uint8_t i2c_addr = 0x27;
int16_t offset_x=0, offset_y=0, offset_z=0;

//Fill in the platform read/write function

#define SLAW    0x4e
#define SLAR    0x4f
// #define SLAW    0x4c


int8_t mir3da_register_read(uint8_t addr, uint8_t *data_m, uint8_t len)
{
    // SI2C_ReadNbyte(SLAR,addr,data_m,len);
    // MyI2C_RecvBuffer(SLAR,addr,data_m,len);
    MyI2C_RecvBuffer(SLAW,addr,data_m,len);
    return 0;
}

int8_t mir3da_register_write(uint8_t addr, uint8_t data_m)
{
    // MyI2C_SendBuffer(SLAW,addr,(uint8_t *)&data_m,1);
    MyI2C_Send_Byte(SLAW,addr,data_m);
    return 0;
}

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

//mir3da_init -> mir3da_set_enable -> mir3da_read_raw_data
//Initialization
int8_t mir3da_init(void){
	uint8_t data_m = 0;
	int i;

	for(i=0; i<10; i++){
        delay_ms(100);
		mir3da_register_read(REG_CHIP_ID, &data_m, 1);
        if(data_m != 0x13){
            printf("REG_CHIP_ID read False=%b02x\r\n",data_m);			
        }else{
            break;
        }

	}
    printf("REG_CHIP_ID=%b02x\r\n",data_m);
	// mir3da_register_write(REG_SPI_CONFIG, 0x66);
	delay_ms(20);

	mir3da_register_write(REG_RESOLUTION_RANGE, 0x84);	//0x0F; +/-2G,12bit
	mir3da_register_write(REG_ODR_AXIS, 0xBA);			//PERF LEVEL3 800hz
	mir3da_register_write(REG_MODE_AXIS, 0x80);			//0x11; suspend mode, bw=1/2odr, autosleep disable

	return 0;	    	
}



//Read three axis data, 2g range, counting 12bit, 1G=1000mg=1024lsb
void mir3da_read_raw_data(short *x, short *y, short *z)
{
    #if 0

    float pitch, roll ,yaw,norm;
    uint8_t tmp_data[6] = {0};
    short data_raw[3] = {0};
    short  yz_plane, xz_plane , xy_plane; 
    short gx,gy,gz;
    short tanX,tanY,tanZ;
    short xAngle,yAngle,zAngle;

	mir3da_register_read(REG_ACC_X_LSB, (uint8_t *)&tmp_data, 6);

    *x = ((short)(tmp_data[1] << 8 | tmp_data[0]))>> 4;
    *y = ((short)(tmp_data[3] << 8 | tmp_data[2]))>> 4;
    *z = ((short)(tmp_data[5] << 8 | tmp_data[4]))>> 4;

    uprintf_array((uint8_t *)&tmp_data, 6);
    printf("x=%hd,y=%hd,z==%hd\r\n",*x,*y,*z);


    *x += 128;
    *y -= 160;
    *z += 275;
    printf("2222  x=%hd,y=%hd,z==%hd\r\n",*x,*y,*z);

    data_raw[0] = *x;
    data_raw[1] = *y;
    data_raw[2] = *z;
    printf("x2=%hd,y2=%hd,z2==%hd\r\n",data_raw[0],data_raw[1],data_raw[2]);




    
    gx = *x;
    gy = *y;
    gz = *z;
  
    yz_plane = (short)(sqrt((gy)*(gy)+(gz)*(gz)));
    xz_plane = (short)(sqrt((gx)*(gx)+(gz)*(gz)));
    xy_plane = (short)(sqrt((gx)*(gx)+(gy)*(gy)));

    if(yz_plane==0.0)
    {
        xAngle = 90.0;
    }
    else
    {
        tanX = ((short)(gx))/yz_plane;
        xAngle = atan(tanX)*180.0/3.14;
    }
    
       
    if(xz_plane==0.0)
    {
        yAngle = 90.0; 
    }
    else
    {
        tanY = ((short)(gy))/xz_plane; 
        yAngle = atan(tanY)*180.0/3.14;
    }
    
    if((gz) == 0.0)
    {
        zAngle = 90.0;  
    }
    else
    {
        tanZ = xy_plane/((short)(gz));  
        zAngle = atan(tanZ)*180.0/3.14; 
    }  
    printf("xAngle=%hd,yAngle=%hd,zAngle==%hd\r\n",xAngle,yAngle,zAngle);
    #else
        uint8_t tmp_data[6] = {0};
        mir3da_register_read(REG_ACC_X_LSB, (uint8_t *)&tmp_data, 6);
        uprintf_array(&tmp_data,6);
        *x = ((short)(tmp_data[1] << 8 | tmp_data[0]))>> 4;
        *y = ((short)(tmp_data[3] << 8 | tmp_data[2]))>> 4;
        *z = ((short)(tmp_data[5] << 8 | tmp_data[4]))>> 4;

    #endif


}

void mir3da_read_data(short *x, short *y, short *z)
{
    uint8_t tmp_data[6] = {0};
   
	mir3da_register_read(REG_ACC_X_LSB, tmp_data, 6);

    *x = (((short)(tmp_data[1] << 8 | tmp_data[0])) >> 4) - offset_x;
    *y = (((short)(tmp_data[3] << 8 | tmp_data[2])) >> 4) - offset_y;
    *z = (((short)(tmp_data[5] << 8 | tmp_data[4])) >> 4) - offset_z;
}
/*
  The chip needs to keep horizontal.
*/
void do_difff_calibrate()
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
		delay_ms(5);
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


short startAngel = 0,nowAngel = 0;

short nowAngel_Table[10] = {0};
uint8_t nowAngel_Table_Index = 0;

void Gyro_Record_Direction(void){
    short gryo_x=0,gryo_y=0,gryo_z=0;
    uint8_t i=0;

    mir3da_read_raw_data((short *)&gryo_x,(short *)&gryo_y,(short *)&gryo_z);
    
    printf("X=%b02x,Y=%b02x,Z=%b02x\r\n",gryo_x,gryo_y,gryo_z);
}


void Gyro_Read_Handle(void){
    static uint8_t gryo_delay_Times = 0;
    gryo_delay_Times++;
    if(gryo_delay_Times<20){
        gryo_delay_Times++;
        return;
    }else{
        gryo_delay_Times = 0;
    }

    // short gryo_x=0,gryo_y=0,gryo_z=0;
    // mir3da_read_raw_data((short *)&gryo_x,(short *)&gryo_y,(short *)&gryo_z);
    Gyro_Record_Direction();

    // printf("x2=%hd,y2=%hd,z2=%hd\r\n",gryo_x,gryo_y,gryo_z);


}