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
#define MESH_INIT_RETRY_LIMIT 0
#define MESH_INIT_TIME_LIMIT  30000
#define MESH_TIME_OUT_MS   30000
#define MESH_STATUS_CHECK_MS  1000

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
#define MESH_SSID_PREFIX "ESP_MESH"
#define MESH_AUTH AUTH_WPA2_PSK //AUTH_OPEN //AUTH_WPA_WPA2_PSK
#define MESH_PASSWORD  "123456789"
void user_mesh_init();
void user_set_mesh_info();
void mesh_reconn_stop();

#endif
#endif
