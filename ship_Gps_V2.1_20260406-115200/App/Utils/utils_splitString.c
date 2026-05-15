#include "utils_splitString.h"

#if 0
uint16_t split_string_ByChar(uint8_t * src, uint8_t ch, uint8_t * dest, uint8_t * index, uint16_t max){
	uint16_t i=0;
	uint16_t count=1;
	
	index[0]=0;

	if(dest==NULL){
		while(*src){
			if(*src == ch){
				if(count<max){
					index[count]=i+1;
                    *src=0;
				    count++;
                    if(count==max){
                        break;
                    }
                }
			}
			src++;
			i++;
		}
	}else{
		while(*src){
			if(*src == ch){
				if(count<max){
					index[count]=i+1;
					*dest=0;
					count++;
					if(count==max){
						break;
					}
				}	
			}else{
				*dest = *src;
			}
			src++;
			dest++;
			i++;
		}
		*dest=0;
	}
	return count;
}

#endif

uint8_t get_NTime_String_ByChar(uint8_t *data_m,uint16_t dataLen,uint8_t flagchar,uint8_t ntime,uint8_t *saveTable,uint8_t maxLen){
    uint8_t startIndex = 0,endIndex = 0,flagNTimes = 0,cpyLen = 0;
	uint16_t i=0;
    for(;i<dataLen;i++){
        if(data_m[i]==flagchar){
            flagNTimes++;
        }
        if(flagNTimes==ntime && data_m[i]==flagchar){
            startIndex = i+1;
        }else if(flagNTimes==(ntime+1) && data_m[i]==flagchar){
            endIndex = i;
            break;
        }
    }
    if(startIndex == 0 && endIndex == 0){
        return 0;
    }else if(startIndex > 0 && endIndex==0){
        endIndex = dataLen-1;
    }
    cpyLen = ((endIndex-startIndex)<= maxLen)?endIndex-startIndex:maxLen;
    strncpy((char *)saveTable,(char *)data_m+startIndex,cpyLen);
    return endIndex-startIndex;
}

