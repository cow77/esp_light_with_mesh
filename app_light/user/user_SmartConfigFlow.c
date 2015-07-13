#include "c_types.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "smartconfig.h"
#include "user_SmartConfigFlow.h"
#include "user_light_hint.h"
#include "user_light_adj.h"
#include "user_light.h"
#include "mem.h"

#if ESP_TOUCH_SUPPORT

ESPTOUCH_PROC  esptouch_proc;
os_timer_t esptouch_tout_t;
LOCAL bool esptouch_ack_flag = false;
#define SC_INFO(STR) os_printf("DBG SMTCONFIG: "STR"\r\n");_LINE_DESP();

bool esptouch_getAckFlag()
{
	return esptouch_ack_flag;
}

void esptouch_setAckFlag(bool flg)
{
	esptouch_ack_flag = flg;
}

LOCAL void esptouch_proc_cb(sc_status status, void *pdata)
{
	switch(status) {
		case SC_STATUS_WAIT:
			SC_INFO("SC_STATUS_WAIT\n");
			break;
		case SC_STATUS_FIND_CHANNEL:
			SC_INFO("SC_STATUS_FIND_CHANNEL\n");
			if(esptouch_proc.esptouch_start_act){
				esptouch_proc.esptouch_start_act(NULL);
			}
			break;
		case SC_STATUS_GETTING_SSID_PSWD:
			SC_INFO("SC_STATUS_GETTING_SSID_PSWD\n");
			#if LIGHT_DEVICE
			light_shadeStart(HINT_BLUE,1000);
			#endif
			if(esptouch_proc.esptouch_fail_cb){
				os_timer_disarm(&esptouch_tout_t);
				os_timer_setfn(&esptouch_tout_t,esptouch_proc.esptouch_fail_cb,NULL);
				os_timer_arm(&esptouch_tout_t,ESP_TOUCH_TIME_OUT_MS,0);
			}
			break;
		case SC_STATUS_LINK:
			SC_INFO("SC_STATUS_LINK\n");
			struct station_config *sta_conf = pdata;
			wifi_station_set_config(sta_conf);
			wifi_station_disconnect();
			wifi_station_connect();
			os_timer_disarm(&esptouch_tout_t);
			os_timer_arm(&esptouch_tout_t,CONNECT_TIME_OUT_MS,0);
			#if LIGHT_DEVICE
			light_blinkStart(HINT_WHITE);
			#endif
			break;
		case SC_STATUS_LINK_OVER:
			os_timer_disarm(&esptouch_tout_t);
			SC_INFO("SC_STATUS_LINK_OVER\n");
			if (esptouch_proc.esptouch_type == SC_TYPE_ESPTOUCH) {
				uint8 phone_ip[4] = {0};
				os_memcpy(phone_ip, (uint8*)pdata, 4);
				os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
			}
			smartconfig_stop();
			SC_INFO("UPDATE PASSWORD HERE\r\n");
			if(esptouch_proc.esptouch_suc_cb){
			    esptouch_proc.esptouch_suc_cb(NULL);//run finish cb
			}
			break;
	}
	
}

void ICACHE_FLASH_ATTR
	esptouch_timer_stop()
{
	os_timer_disarm(&esptouch_tout_t);
	#if LIGHT_DEVICE
	light_hint_stop(HINT_WHITE);
	#endif
}

void ICACHE_FLASH_ATTR
	esptouch_finishCb(void* data)
{
	os_timer_disarm(&esptouch_tout_t);//disable check timeout 
	//light_shadeStart(HINT_WHITE,2000);
	#if LIGHT_DEVICE
	light_hint_stop(HINT_WHITE);
	#endif
	SC_INFO("ESP-TOUCH SUCCESS \r\n");
	
	SC_INFO("ENABLE LIGHT ACTION(ESP-NOW)");
	os_printf("debug: channel:%d\r\n",wifi_get_channel());
#if ESP_NOW_SUPPORT
	light_action_init();
#endif
	SC_INFO("CONNECTED TO AP...ENABLE SOFTAP&WEBSERVER ...WAIT...\r\n");
#if ESP_MESH_SUPPORT
	user_mesh_init();
#endif
}

void ICACHE_FLASH_ATTR
	esptouch_fail_callback(void* data)
{	
	wifi_station_disconnect();
	smartconfig_stop();
	wifi_set_opmode(STATIONAP_MODE);
	
	SC_INFO("ESP-TOUCH FAIL \r\n");
	os_timer_disarm(&esptouch_tout_t);
	#if LIGHT_DEVICE
	light_shadeStart(HINT_RED,2000);
	#endif
	
	SC_INFO("ENABLE LIGHT ACTION(ESP-NOW)");
	os_printf("debug: channel:%d\r\n",wifi_get_channel());
#if ESP_NOW_SUPPORT
	light_action_init();
#endif

#if ESP_MESH_SUPPORT
	SC_INFO("ESP-TOUCH FAIL, OPEN WEBSERVER NOW");
	mesh_set_softap();//check
	SC_INFO("RESTART MESH NOW...\r\n");
	#if LIGHT_DEVICE
	light_hint_stop(HINT_RED);
	#endif
	user_mesh_init();
#endif
}

void ICACHE_FLASH_ATTR
	esptouch_startAct(void* para)
{
    SC_INFO("LIGHT SHADE & START ESP-TOUCH");
	#if LIGHT_DEVICE
	light_shadeStart(HINT_GREEN,1000);
	#endif
}

void ICACHE_FLASH_ATTR
	esptouch_flow_init()
{
#if ESP_NOW_SUPPORT
	light_action_deinit();
#endif
	wifi_disable_check_ip();
	esptouch_setAckFlag(true);
	
	SC_INFO("ESP-TOUCH FLOW INIT...\r\n");
	esptouch_proc.esptouch_fail_cb = esptouch_fail_callback;
	esptouch_proc.esptouch_start_act = esptouch_startAct;
	esptouch_proc.esptouch_suc_cb = esptouch_finishCb;
	esptouch_proc.esptouch_type = SC_TYPE_ESPTOUCH;
	
    SC_INFO("ESP-TOUCH SET STATION MODE ...\r\n");
    wifi_set_opmode(STATION_MODE);

	SC_INFO("ESP-TOUCH START");
	smartconfig_start(esptouch_proc_cb);
    if(esptouch_proc.esptouch_fail_cb){
    	os_timer_disarm(&esptouch_tout_t);
    	os_timer_setfn(&esptouch_tout_t,esptouch_proc.esptouch_fail_cb,NULL);
    	os_timer_arm(&esptouch_tout_t,ESP_TOUCH_TIME_ENTER,0);
    }

}

void ICACHE_FLASH_ATTR
	esptouch_flow_stop()
{
    esptouch_timer_stop();
	smartconfig_stop();
}



#endif

