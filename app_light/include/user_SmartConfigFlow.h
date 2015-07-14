#ifndef _SMARTCONFIG_FLOW_H
#define _SMARTCONFIG_FLOW_H
#include "smartconfig.h"
typedef void (*esptouch_StartAction)(void *para);
typedef void (*esptouch_FailCallback)(void *para);
typedef void (*esptouch_SuccessCallback)(void *para);

typedef struct  {
	sc_type esptouch_type;	
	esptouch_StartAction esptouch_start_cb;
	esptouch_FailCallback esptouch_fail_cb;
	esptouch_SuccessCallback esptouch_suc_cb;
} ESPTOUCH_PROC;


//Time limit for connecting WiFi after ESP-TOUCH figured out the SSID&PWD
#define ESPTOUCH_CONNECT_TIMEOUT_MS 40000
//Time limit for ESP-TOUCH to receive config packets
#define ESP_TOUCH_TIME_ENTER  20000
//Total time limit for ESP-TOUCH
#define ESP_TOUCH_TIMEOUT_MS 60000

void esptouch_FlowStart();



#endif
