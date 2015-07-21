#ifndef USER_LIGHT_MESH_H
#define USER_LIGHT_MESH_H
#include "user_config.h"
#if ESP_MESH_SUPPORT

#include "os_type.h"
#include "eagle_soc.h"
#include "c_types.h"
#include "osapi.h"
#include "ets_sys.h"
#include "mem.h"
#include "user_light_adj.h"
#include "user_light.h"
#include "user_interface.h"


#define MESH_INFO os_printf

//number of retry attempts after mesh enable failed;
#define MESH_INIT_RETRY_LIMIT 0
//limit time of mesh init
#define MESH_INIT_TIME_LIMIT  30000
#define MESH_TIME_OUT_MS   30000
//time expire to check mesh init status
#define MESH_STATUS_CHECK_MS  1000
//length of binary upgrade stream in a single packet
#define MESH_UPGRADE_SEC_SIZE 640

typedef void (*mesh_FailCallback)(void *para);
typedef void (*mesh_SuccessCallback)(void *para);
typedef void (*mesh_InitTimeoutCallback)(void *para);

typedef struct  {
	mesh_FailCallback mesh_fail_cb;
	mesh_SuccessCallback mesh_suc_cb;
	mesh_InitTimeoutCallback mesh_init_tout_cb;
	uint32 start_time;
	uint32 init_retry;
} LIGHT_MESH_PROC;


/*SET THE DEFAULT MESH SSID PATTEN*/
/*THE FINAL SSID OF SOFTAP WOULD BE "MESH_SSID_PREFIX_X_YYYYYY"*/
#define MESH_SSID_PREFIX "TEST_MESH"
#define MESH_AUTH AUTH_WPA2_PSK //AUTH_OPEN //AUTH_WPA_WPA2_PSK
#define MESH_PASSWORD  "123123123"


void user_MeshInit();
void user_MeshSetInfo();
void mesh_StopReconnCheck();
char* mesh_GetMdevMac();


#endif
#endif
