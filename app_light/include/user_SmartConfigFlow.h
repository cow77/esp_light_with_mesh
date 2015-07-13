#ifndef _SMARTCONFIG_FLOW_H
#define _SMARTCONFIG_FLOW_H
#include "smartconfig.h"
typedef void (*esptouch_StartAction)(void *para);
typedef void (*esptouch_FailCallback)(void *para);
typedef void (*esptouch_SuccessCallback)(void *para);

typedef struct  {
	sc_type esptouch_type;	
	esptouch_StartAction esptouch_start_act;
	esptouch_FailCallback esptouch_fail_cb;
	esptouch_SuccessCallback esptouch_suc_cb;
} ESPTOUCH_PROC;


#define CONNECT_TIME_OUT_MS 40000
#define ESP_TOUCH_TIME_ENTER  20000
#define ESP_TOUCH_TIME_OUT_MS 60000

void esptouch_flow_init();



#endif
