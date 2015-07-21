#ifndef _USER_LIGHT_ACTION_H
#define _USER_LIGHT_ACTION_H

#include "user_config.h"
#include "os_type.h"

enum{
COLOR_SET = 0,
COLOR_CHG ,
COLOR_TOGGLE,
COLOR_LEVEL,
LIGHT_RESET
};


#define CMD_NUM 10
#define LIGHT_DEV_NUM  10



/*right now , all the MAC address is hard coded and can not be changed */
/*One controller is bind to the certain groups of MAC address.*/
/*next , we will add a encryption for ESP-NOW.*/
/* and send broadcast instead on unicast.*/
#define ESPNOW_ENCRYPT  1
#define ESPNOW_KEY_HASH 1

#define ESPNOW_KEY_LEN 16
uint8 esp_now_key[ESPNOW_KEY_LEN] = {0x10,0xfe,0x94, 0x7c,0xe6,0xec,0x19,0xef,0x33, 0x9c,0xe6,0xdc,0xa8,0xff,0x94, 0x7d};//key

#define DEV_MAC_LEN 6
const uint8 SWITCH_MAC[DEV_MAC_LEN] = {0x18,0xfe,0x34, 0xa5,0x3d,0x68};
const uint8 LIGHT_MAC[LIGHT_DEV_NUM][DEV_MAC_LEN] = {
	                                                 
	                                                 {0x1a,0xfe,0x34, 0xa1,0x32,0xaa},	
													 {0x1a,0xfe,0x34, 0x98,0x03,0xff},
													 
												     //{0x1a,0xfe,0x34, 0x9a,0xa3,0xcd},
	                                                 {0x1a,0xfe,0x34, 0xa0,0xd6,0x97},
	                                                 {0x1a,0xfe,0x34, 0x9f,0xf3,0x68},	                                                 
	                                                 {0x1a,0xfe,0x34, 0xa0,0xac,0x19},
	                                                 {0x1a,0xfe,0x34, 0xa1,0x08,0x25},
	                                                 {0x1a,0xfe,0x34, 0xa1,0x09,0x3a},
	                                                 {0x1a,0xfe,0x34, 0xa1,0x06,0x66},												
	                                                 {0x1a,0xfe,0x34, 0xa1,0x07,0x47},
	                                                 {0x1a,0xfe,0x34, 0xa1,0x08,0x1a},
	                                                 
	                                                 
                                            };





#if LIGHT_DEVICE
void light_EspnowInit();
void light_EspnowDeinit();
#elif LIGHT_SWITCH
void  switch_EspnowInit();
void  switch_EspnowSendLightCmd(uint16 idx, uint16 channelNum, uint32* duty, uint32 period,uint32 code);
void switch_EspnowSendCmdByChnl(uint16 chn,uint16 channelNum, uint32* duty, uint32 period,uint32 code);
void switch_EspnowSendChnSync(uint8 channel);
void  switch_EspnowDeinit();
#endif










#endif
