#include "user_light_mesh.h"
#include "user_light_hint.h"
#if ESP_MESH_SUPPORT
#include "mesh.h"

LIGHT_MESH_PROC LightMeshProc;
os_timer_t mesh_check_t;
os_timer_t mesh_tout_t;
os_timer_t mesh_user_t;


void ICACHE_FLASH_ATTR
	meshStopCheckTimer()
{
	os_timer_disarm(&mesh_check_t);
	os_timer_disarm(&mesh_tout_t);
}

void ICACHE_FLASH_ATTR
    mesh_set_softap()
{
    MESH_INFO("----------------------\r\n");
	MESH_INFO("MESH ENABLE SOFTAP \r\n");
	MESH_INFO("----------------------\r\n");

    struct softap_config config_softap;
    char ssid[33]={0};

    wifi_softap_get_config(&config_softap);
    os_memset(config_softap.password, 0, sizeof(config_softap.password));
    os_memset(config_softap.ssid, 0, sizeof(config_softap.ssid));
    os_sprintf(ssid,"ESP_%06X",system_get_chip_id());
    os_memcpy(config_softap.ssid, ssid, os_strlen(ssid));
    config_softap.ssid_len = os_strlen(ssid);
	
	config_softap.ssid_hidden = 0;
	config_softap.channel = wifi_get_channel();
	
#ifdef SOFTAP_ENCRYPT
    char password[33];
	char macaddr[6];

    os_sprintf(password, MACSTR "_%s", MAC2STR(macaddr), PASSWORD);
    os_memcpy(config_softap.password, password, os_strlen(password));
    config_softap.authmode = AUTH_WPA_WPA2_PSK;

#else
    os_memset(config_softap.password,0,sizeof(config_softap.password));
    config_softap.authmode = AUTH_OPEN;
#endif

    wifi_softap_set_config(&config_softap);
	wifi_set_opmode(STATIONAP_MODE);
	
    wifi_softap_get_config(&config_softap);
	MESH_INFO("SSID: %s \r\n",config_softap.ssid);
	MESH_INFO("CHANNEL: %d \r\n",config_softap.channel);
	MESH_INFO("-------------------------\r\n");

}


///enable callback will only be called if the device have joined a mesh network
void ICACHE_FLASH_ATTR
	mesh_enable_cb()
{
	meshStopCheckTimer();

	_LINE_DESP();
	MESH_INFO("TEST IN MESH ENABLE CB\r\n");
    MESH_INFO("%s\n", __func__);
	MESH_INFO("HEAP: %d \r\n",system_get_free_heap_size());
	_LINE_DESP();
	if(LightMeshProc.mesh_suc_cb){
		LightMeshProc.mesh_suc_cb(NULL);
	}
}

void ICACHE_FLASH_ATTR
	espconn_mesh_disable_cb()
{
	_LINE_DESP();
	MESH_INFO("TEST IN MESH DISABLE CB\r\n");
    MESH_INFO("%s\n", __func__);
	MESH_INFO("HEAP: %d \r\n",system_get_free_heap_size());
	_LINE_DESP();
}

void ICACHE_FLASH_ATTR
	meshStart(void* arg)
{
	MESH_INFO("mesh log: mesh start here\r\n");
	user_esp_platform_connect_ap_cb();
	return;
}

void ICACHE_FLASH_ATTR
	meshSuccess(void* arg)
{
	_LINE_DESP();
	MESH_INFO("mesh log: mesh success!\r\n");
	MESH_INFO("CONNECTED, DO RUN ESP PLATFORM...\r\n");
	MESH_INFO("mesh status: %d\r\n",espconn_mesh_get_status());
	_LINE_DESP();
	//init ESP-NOW ,so that light can be controlled by ESP-NOW SWITCHER.
	
#if ESP_NOW_SUPPORT
	light_action_init();
#endif
	wifi_enable_check_ip();
	//run esp-platform procedure,register to server.
	user_esp_platform_connect_ap_cb();
	return;
}

void ICACHE_FLASH_ATTR
	meshFail(void* arg)
{
	MESH_INFO("mesh fail\r\n");
	meshStopCheckTimer();
#if ESP_NOW_SUPPORT
	//initialize ESP-NOW
	light_action_init();
#endif
	MESH_INFO("CALL MESH DISABLE HERE...\r\n");
	//stop or disable mesh
	espconn_mesh_disable(espconn_mesh_disable_cb);
	//open softap interface
	mesh_set_softap();
	//try AP CACHE and connect
	wifi_ap_scan_start();
	return;
	
}

void ICACHE_FLASH_ATTR
	meshTimeout(void* arg)
{
	MESH_INFO("MESH INIT TIMEOUT, STOP MESH\r\n");
	meshFail(NULL);
	return;
}

uint32 ICACHE_FLASH_ATTR
	mesh_get_start_ms()
{
	return (system_get_time()-LightMeshProc.start_time)/1000;
}


void ICACHE_FLASH_ATTR
	mesh_status_check()
{
    os_timer_disarm(&mesh_check_t);
	sint8 mesh_status = espconn_mesh_get_status();
	MESH_INFO("--------------\r\n");
	MESH_INFO("mesh status: %d ; %d\r\n",mesh_status,system_get_free_heap_size());
	
	MESH_INFO("--------------\r\n");
	
	if(mesh_status == MESH_DISABLE){
		MESH_INFO("MESH DISABLE , RUN FAIL CB ,retry:%d \r\n",LightMeshProc.init_retry);
		if(LightMeshProc.init_retry<MESH_INIT_RETRY_LIMIT && (mesh_get_start_ms()<MESH_INIT_TIME_LIMIT)){
			LightMeshProc.init_retry+=1;
			espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
			MESH_INFO("MESH RETRY : %d \r\n",LightMeshProc.init_retry);
		}else{
			meshStopCheckTimer();
			MESH_INFO("MESH INIT RETRY FAIL...\r\n");
			if(LightMeshProc.mesh_fail_cb){
				LightMeshProc.mesh_fail_cb(NULL);
			}
			LightMeshProc.init_retry = 0;
			return;
		}
	}
	else if(mesh_status==MESH_NET_CONN){
		MESH_INFO("MESH WIFI CONNECTED\r\n");
		meshStopCheckTimer();
	}
	os_timer_arm(&mesh_check_t,MESH_STATUS_CHECK_MS,0);
}

void ICACHE_FLASH_ATTR
	mesh_recon_check()
{

	MESH_INFO("---------\r\n");
	MESH_INFO("mesh_recon_check\r\n");
	MESH_INFO("---------\r\n");

	os_timer_disarm(&mesh_user_t);
	sint8 mesh_status = espconn_mesh_get_status();
	MESH_INFO("MESH STATUS CHECK: %d \r\n",mesh_status);

	if(mesh_status == MESH_ONLINE_AVAIL){
		MESH_INFO("MESH ONLINE AVAIL\r\n");
		user_esp_platform_sent_data();
    }	//if(mesh_status == MESH_LOCAL_AVAIL){
    else{
		espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
		os_timer_arm(&mesh_user_t,20000,1);
	}
}

void ICACHE_FLASH_ATTR
	mesh_reconn_stop()
{
	MESH_INFO("---------\r\n");
	MESH_INFO("mesh_reconn_stop\r\n");
	MESH_INFO("---------\r\n");
    os_timer_disarm(&mesh_user_t);
}

void ICACHE_FLASH_ATTR
	user_mesh_start(enum mesh_type type)
{
    os_timer_disarm(&mesh_user_t);
	os_timer_setfn(&mesh_user_t,mesh_recon_check,NULL);
	os_timer_arm(&mesh_user_t,10000,0);
}


/*Set callback function and start mesh.
  MESH_ONLINE: CAN BE CONTROLLED BY WAN SERVER,AS WELL AS LAN APP VIA ROUTER.
  MESH_LOCAL: CAN ONLY BE CONTROLLED BY LOCAL NETWORK.

*/
void ICACHE_FLASH_ATTR
	user_mesh_init()
{
	LightMeshProc.mesh_suc_cb=meshSuccess;
	LightMeshProc.mesh_fail_cb=meshFail;
	LightMeshProc.mesh_init_tout_cb=meshTimeout;
	LightMeshProc.start_time = system_get_time();
	LightMeshProc.init_retry = 0;

    os_printf("test: %s\n", __func__);
    espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
	if(LightMeshProc.mesh_init_tout_cb){
    	os_timer_disarm(&mesh_tout_t);
    	os_timer_setfn(&mesh_tout_t,LightMeshProc.mesh_init_tout_cb,NULL);
    	os_timer_arm(&mesh_tout_t,MESH_TIME_OUT_MS,0);
	}
	os_timer_disarm(&mesh_check_t);
	os_timer_setfn(&mesh_check_t,mesh_status_check,NULL);
	os_timer_arm(&mesh_check_t,MESH_STATUS_CHECK_MS,0);
}


void ICACHE_FLASH_ATTR
	user_set_mesh_info()
{
	if( espconn_mesh_set_ssid_prefix(MESH_SSID_PREFIX,os_strlen(MESH_SSID_PREFIX))){
		MESH_INFO("SSID PREFIX SET OK..\r\n");
	}else{
		MESH_INFO("SSID PREFIX SET ERROR..\r\n");
	}

	if(espconn_mesh_encrypt_init(MESH_AUTH,MESH_PASSWORD,os_strlen(MESH_PASSWORD))){
		MESH_INFO("SOFTAP ENCRYPTION SET OK..\r\n");
	}else{
		MESH_INFO("SOFTAP ENCRYPTION SET ERROR..\r\n");
	}
}

#endif

