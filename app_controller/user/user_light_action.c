
#include "os_type.h"
#include "eagle_soc.h"
#include "c_types.h"
#include "osapi.h"
#include "ets_sys.h"
#include "mem.h"
#include "user_light_action.h"
#include "user_switch.h"
#include "user_interface.h"
#include "espnow.h"

//The espnow packet struct used for light<->switch intercommunications
typedef struct{
    uint16 battery_voltage_mv;
    uint16 battery_status;
}EspnowBatStatus;

typedef struct {
	uint16 pwm_num;
	uint32 period;
	uint32 duty[8];
	uint32 cmd_code;
	//uint8 color_idx;
	//uint16 battery_voltage_mv;
	//uint16 battery_status;
    EspnowBatStatus batteryStat;
	//uint32 check_sum; //move to outside
} EspnowLightCmd;

typedef struct{
    uint8 ip_addr[4];
	uint8 channel;
	uint8 meshStatus;
	uint8 meshLevel;
	uint8 platformStatus;
	uint8 espCloudConnIf;
	uint8 espCloudRegIf;
	uint8 devKey[40];
}EspnowLightRsp;

typedef struct{
    uint8 switchChannel;
	uint8 lightChannel;
    EspnowBatStatus batteryStat;
}EspnowLightSync;


typedef enum{
    ACT_IDLE = 0,
    ACT_ACK = 1,
    ACT_REQ = 2,
    ACT_TIME_OUT =3,
    ACT_RSP=4,
}EspnowMsgStatus;

typedef enum{
    ACT_TYPE_DATA = 0,
    ACT_TYPE_ACK = 1,//NOT USED, REPLACED BY MAC ACK
    ACT_TYPE_SYNC = 2,
    ACT_TYPE_RSP = 3,
}EspnowMsgType;


typedef enum {
	ACT_BAT_NA = 0,
	ACT_BAT_OK = 1,
	ACT_BAT_EMPTY = 2
}ACT_BAT_TYPE;


typedef struct{
    uint8 csum;
    uint8 type;
    uint32 token;//for encryption, add random number to 
    uint16 cmd_index;
	uint8 wifiChannel;
	uint32 sequence;
    union{
    EspnowLightCmd lightCmd;
    EspnowLightRsp lightRsp;
    EspnowLightSync lightSync;
    };
}EspnowProtoMsg;

#define BAT_EMPTY_MV 2100

#define ACT_DEBUG 1
#define ACT_PRINT os_printf
#define WIFI_DEFAULT_CHANNEL 1


#ifdef LIGHT_SWITCH

#define ACTION_RETRY_NUM  3
#define ACTION_RETRY_TIMER_MS  100
typedef void (*ActionToutCallback)(uint32* act_idx);

typedef struct{
    uint32 sequence;
    EspnowMsgStatus status;
    os_timer_t req_timer;
    uint32 retry_num;
    uint32 retry_expire;
    ActionToutCallback actionToutCb;
    EspnowProtoMsg EspnowMsg;
    uint16 wifichannel;
}Action_SendStatus;


typedef struct{
    uint32 magic;
    uint16 wifiChannel[LIGHT_DEV_NUM];
}FlashData;

os_timer_t action_retry_t;
Action_SendStatus actionReqStatus[LIGHT_DEV_NUM] = {0};
FlashData flashData_t;
#define FLASH_PARAM_SEC 0X7D
#define FLASH_DATA_MAGIC 0x5cc5


LOCAL uint8 channel_group[LIGHT_DEV_NUM];
LOCAL uint8 channel_num = 0;
LOCAL uint8 channel_cur = 1;
LOCAL uint8 pwm_chn_num = 5;
LOCAL uint32 pwm_duty[8];
LOCAL uint32 pwm_period;
LOCAL uint32 cmd_code=0;

typedef enum{
	CUR_CHL_OK = 0,
	ALL_CHL_OK,
	CUR_CHL_WAIT,
}EspnowReqRet;
#endif

bool ICACHE_FLASH_ATTR
	light_EspnowCmdValidate(EspnowProtoMsg* EspnowMsg)
{
    uint8* data = (uint8*)EspnowMsg;
    uint8 csum_cal = 0;
    int i;
    for(i=1;i<sizeof(EspnowLightCmd);i++){
		csum_cal+= *(data+i);
    }
    if(csum_cal == EspnowMsg->csum){
        return true;
    }else{
        return false;
    }
}


void ICACHE_FLASH_ATTR
	light_EspnowSetCsum(EspnowProtoMsg* EspnowMsg)
{
    uint8* data = (uint8*)EspnowMsg;
    uint8 csum_cal = 0;
    int i;
    for(i=1;i<sizeof(EspnowLightCmd);i++){
		csum_cal+= *(data+i);
    }
    EspnowMsg->csum= csum_cal;
}

#if LIGHT_DEVICE
#define LIGHT_DEBUG 1
void ICACHE_FLASH_ATTR light_EspnowRcvCb(u8 *macaddr, u8 *data, u8 len)
{
    int i;
    uint32 duty_set[PWM_CHANNEL] = {0};
    uint32 period=1000;
    
    EspnowLightCmd light_cmd;
    os_memcpy((uint8*)(&light_cmd), data,sizeof(EspnowLightCmd));
    ACT_PRINT("RCV DATA: \r\n");
    for(i=0;i<len;i++) ACT_PRINT("%02x ",data[i]);
    ACT_PRINT("\r\n-----------------------\r\n");

    if(light_EspnowCmdValidate(&light_cmd) ){
        ACT_PRINT("cmd check sum ok\r\n");
        #if (LIGHT_DEBUG==0)  
        //ACTUALL WE CAN GET THE PRACTIACAL MAC OF THE SOURCE NOW: (*macaddr)
        if (0 == os_memcmp( light_cmd.source_mac, SWITCH_MAC,sizeof(SWITCH_MAC))){
        #else  //debug!!!
        if(true){ //debug
            uint8 mac_bkp[6] = {0};
            os_memcpy(mac_bkp,light_cmd.source_mac,sizeof(mac_bkp));
            uint8 mac_buf[6] = {0};
            //wifi_get_macaddr(STATION_IF,mac_buf);
            wifi_get_macaddr(SOFTAP_IF,mac_buf);
        #endif
            
            //if(light_cmd.wifiChannel == wifi_get_channel()){
                //ACT_TYPE_DATA: this is a set of data or command.
                if(light_cmd.type == ACT_TYPE_DATA){
                    ACT_PRINT("period: %d ; channel : %d \r\n",light_cmd.period,light_cmd.pwm_num);
                    for(i=0;i<light_cmd.pwm_num;i++){
                        ACT_PRINT(" duty[%d] : %d \r\n",i,light_cmd.duty[i]);
                        duty_set[i] = light_cmd.duty[i];
                    }
                    
                    ACT_PRINT("SOURCE MAC CHEK OK \r\n");
                    ACT_PRINT("cmd channel : %d \r\n",light_cmd.wifiChannel);
                    ACT_PRINT("SELF CHANNEL: %d \r\n",wifi_get_channel());
                    ACT_PRINT("Battery status %d voltage %dmV\r\n", light_cmd.battery_status, light_cmd.battery_voltage_mv);
                    light_action_store_battery_status(light_cmd.source_mac, light_cmd.battery_status, light_cmd.battery_voltage_mv);
                    light_set_aim(duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],light_cmd.period,0);
                    
                    light_cmd.type = ACT_TYPE_RSP;
                    light_cmd.wifiChannel = wifi_get_channel();
                    os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
                    ACT_PRINT("send ack \r\n");
                }
                //ACT_TYPE_SYNC: AFTER LIGHT CONFIGURED TO ROUTER, ITS CHANNEL CHANGES
                //               NEED TO SET THE CHANNEL FOR EACH LIGHT.
                else if(light_cmd.type == ACT_TYPE_SYNC && (light_cmd.wifiChannel == wifi_get_channel())){
                    light_cmd.type = ACT_TYPE_SYNC;
                    ACT_PRINT("cmd rcv channel : %d \r\n",light_cmd.wifiChannel);
                    light_cmd.wifiChannel = wifi_get_channel();
                    os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
                    ACT_PRINT("send sync, self channel : %d  \r\n",wifi_get_channel());
                }else{
                    ACT_PRINT(" data type %d \r\n",light_cmd.type);
                    ACT_PRINT("data channel :%d \r\n",light_cmd.wifiChannel);
                    ACT_PRINT("data self channel: %d \r\n",wifi_get_channel());
                    ACT_PRINT("data type or channel error\r\n");
                    return;
                }
                
                light_EspnowSetCsum(&light_cmd);
            #if ACT_DEBUG
                int j;
                for(j=0;j<sizeof(EspnowLightCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
                ACT_PRINT("\r\n");
            #endif
            #if (LIGHT_DEBUG==0)
                ACT_PRINT("send to mac: %02x %02x %02x %02x %02x %02x\r\n",
                SWITCH_MAC[0],SWITCH_MAC[1],SWITCH_MAC[2],SWITCH_MAC[3],SWITCH_MAC[4],SWITCH_MAC[5]);
                esp_now_send(SWITCH_MAC, (uint8*)(&light_cmd), sizeof(EspnowLightCmd)); //send ack
            #else
                ACT_PRINT("light debug: send to mac: %02x %02x %02x %02x %02x %02x\r\n",
                mac_bkp[0],mac_bkp[1],mac_bkp[2],mac_bkp[3],mac_bkp[4],mac_bkp[5]);
                esp_now_send(mac_bkp, (uint8*)(&light_cmd), sizeof(EspnowLightCmd)); //send ack
            #endif
            //}
        }else{
            ACT_PRINT("SOURCE MAC CHEK FAIL \r\n");
        }
    }else{
        ACT_PRINT("cmd check sum error\r\n");
    }
}

void ICACHE_FLASH_ATTR light_EspnowInit()
{
	_LINE_DESP();
    ACT_PRINT("CHANNEL : %d \r\n",wifi_get_channel());
	_LINE_DESP();
    
    if (esp_now_init()==0) {
        ACT_PRINT("direct link  init ok\n");
        esp_now_register_recv_cb(light_EspnowRcvCb);
    } else {
        ACT_PRINT("esp-now already init\n");
    }
    //send data from station interface of switch to softap interface of light
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);  //role 1: switch   ;  role 2 : light;
    esp_now_set_kok(esp_now_key, ESPNOW_KEY_LEN);

	    int j;
	    for(j=0;j<SWITCH_DEV_NUM;j++){
			
#if ESPNOW_ENCRYPT
			esp_now_add_peer((uint8*)SWITCH_MAC[j], (uint8)ESP_NOW_ROLE_CONTROLLER,(uint8)WIFI_DEFAULT_CHANNEL, esp_now_key, (uint8)ESPNOW_KEY_LEN);
#else
    	    esp_now_add_peer((uint8*)SWITCH_MAC[j], (uint8)ESP_NOW_ROLE_CONTROLLER,(uint8)WIFI_DEFAULT_CHANNEL, NULL, (uint8)ESPNOW_KEY_LEN);
#endif
		}

}

void ICACHE_FLASH_ATTR light_EspnowDeinit()
{
    esp_now_unregister_recv_cb(); 
    esp_now_deinit();
}


#elif LIGHT_SWITCH

EspnowReqRet ICACHE_FLASH_ATTR
switch_CheckCmdResult()
{
    int i;
    EspnowReqRet ret = CUR_CHL_OK;// ALL_CHL_OK;
    for(i=0;i<CMD_NUM;i++){
        //os_printf("actionReqStatus[channel_group[i]]:%d\r\n",actionReqStatus[channel_group[i]]);
        if(actionReqStatus[channel_group[i]].status == ACT_REQ) return CUR_CHL_WAIT;
        if(actionReqStatus[channel_group[i]].status == ACT_TIME_OUT) ret = CUR_CHL_OK;
    }
    return ret;
}


void ICACHE_FLASH_ATTR
switch_EspnowAckCb()
{
    EspnowReqRet ret = switch_CheckCmdResult();
    
    if( ret==CUR_CHL_OK){
        if(channel_cur == 14){
            ACT_PRINT("release power\r\n");
            _SWITCH_GPIO_RELEASE();
        }else{
            ACT_PRINT("NEXT CHANNEL: %d \r\n",++channel_cur);
            switch_EspnowSendCmdByChnl(channel_cur,pwm_chn_num, pwm_duty, pwm_period,cmd_code);
        }
    
    }else if(ret == ALL_CHL_OK){
        ACT_PRINT("ALL CHANNEL OK,release power\r\n");
        _SWITCH_GPIO_RELEASE();
    }
    
}



EspnowReqRet ICACHE_FLASH_ATTR
switch_CheckSyncResult()
{
    int i;
    EspnowReqRet ret = ALL_CHL_OK;
    for(i=0;i<CMD_NUM;i++){ //CMD NUM QUEALS PRACTICAL LIGHT NUMBER
        //USING MAC ACK CB, SYNC NEED A ACT_RSP
        if(actionReqStatus[i].status == ACT_REQ || actionReqStatus[i].status == ACT_ACK ) return CUR_CHL_WAIT;
        if(actionReqStatus[i].status==ACT_TIME_OUT)	ret=CUR_CHL_OK ;
    }
    os_printf("%s ret: %d \r\n",__func__,ret);
    return ret;
}


void ICACHE_FLASH_ATTR
switch_EspnowSyncExit()
{
    ACT_PRINT("release power\r\n");
    int i;
    for(i=0;i<CMD_NUM;i++){
        flashData_t.magic = FLASH_DATA_MAGIC;
        flashData_t.wifiChannel[i] = actionReqStatus[i].wifichannel;
    }
    system_param_save_with_protect(FLASH_PARAM_SEC,&flashData_t,sizeof(flashData_t));
    _SWITCH_GPIO_RELEASE();
}


void ICACHE_FLASH_ATTR
switch_EspnowSyncCb()
{
    EspnowReqRet ret = switch_CheckSyncResult();
    os_printf("ACT RET: %d \r\n",ret);
    if(ret==CUR_CHL_OK){
        os_printf("SYNC FINISHED ...\r\n");
        if(channel_cur == 14){
            switch_EspnowSyncExit();
        }else{
            ACT_PRINT("SYNC NEXT CHANNEL: %d \r\n",++channel_cur);
            //switch_EspnowSendCmdByChnl(channel_cur,pwm_chn_num, pwm_duty, pwm_period);
            switch_EspnowSendChnSync(channel_cur);
        }
    }else if(ret == ALL_CHL_OK){
        ACT_PRINT("ALL CHANNEL OK,\r\n");
        switch_EspnowSyncExit();
    }
}




void ICACHE_FLASH_ATTR
switch_EspnowSendRetry(void* arg)
{
    //ACT_PRINT("%s  \r\n",__func__);
    uint16 _idx = *((uint16*)arg);	
    EspnowProtoMsg* EspnowRetryMsg = &(actionReqStatus[_idx].EspnowMsg);
    Action_SendStatus* EspnowSendStatus = &(actionReqStatus[_idx]);
    #if 0
    os_printf("*********************************\r\n");
    os_printf("-----Msg-----\r\n");
    os_printf("actionReqStatus[%d].EspnowMsg.sequence: %d \r\n",_idx,actionReqStatus[_idx].EspnowMsg.sequence);
    os_printf("actionReqStatus[%d].EspnowMsg.type: %d \r\n",_idx,actionReqStatus[_idx].EspnowMsg.type);
    os_printf("-----Status-------\r\n");
    os_printf("actionReqStatus[%d].status : %d \r\n",_idx,actionReqStatus[_idx].status);
    os_printf("actionReqStatus[%d].sequence : %d \r\n",_idx,actionReqStatus[_idx].sequence);
    os_printf("actionReqStatus[%d].retry num: %d \r\n",_idx,actionReqStatus[_idx].retry_num);
    os_printf("*********************************\r\n");
    #endif
    if((EspnowRetryMsg->sequence != EspnowSendStatus->sequence)){
        ACT_PRINT("action updated...,cancel retry ...\r\n");
        return;
    }
    if(EspnowRetryMsg->type==ACT_TYPE_DATA){
        if(EspnowSendStatus->status== ACT_REQ){
            if(EspnowSendStatus->retry_num < ACTION_RETRY_NUM){
                os_printf("retry send data\r\n");
                esp_now_send((uint8*)LIGHT_MAC[_idx], (uint8*)EspnowRetryMsg, sizeof(EspnowProtoMsg));
                EspnowSendStatus->retry_num++;
                //os_timer_arm( &action_status->req_timer, action_status->retry_expire,0);
            }
        }else{
            ACT_PRINT("[%d] ACKed, STATUS  : %d\r\n",_idx,EspnowSendStatus->status);
        }
    }
    else if(EspnowRetryMsg->type==ACT_TYPE_SYNC){
        if(EspnowSendStatus->status== ACT_REQ || EspnowSendStatus->status== ACT_ACK){
            if(EspnowSendStatus->retry_num < ACTION_RETRY_NUM){
                os_printf("retry send sync\r\n");
                esp_now_send((uint8*)LIGHT_MAC[_idx], (uint8*)EspnowRetryMsg, sizeof(EspnowProtoMsg));
                EspnowSendStatus->retry_num++;
                //os_timer_arm( &action_status->req_timer, action_status->retry_expire,0);
            }
        }else{
            ACT_PRINT("[%d] REPed, STATUS  : %d\r\n",_idx,EspnowSendStatus->status);
        }
    }
}

void ICACHE_FLASH_ATTR 
switch_EspnowSendCmdByChnl(uint16 chn,uint16 channelNum, uint32* duty, uint32 period,uint32 code)
{
    int i = 0;
    os_memset(channel_group, 0 ,sizeof(channel_group));
    channel_num = 0;
    channel_cur = chn;
    cmd_code = code;
    pwm_period = period;
    os_memcpy(pwm_duty,duty,sizeof(pwm_duty));
    pwm_chn_num = channelNum;
    
    for(i=0;i<CMD_NUM;i++){
        if(actionReqStatus[i].wifichannel == chn){
            channel_group[channel_num++]=i;
            ACT_PRINT("CHANNEL %d : add idx %d\r\n",chn,i);
        }
    }
    #if 0
    os_printf("********************\r\n");
    os_printf("cur chn: %d ; chn num: %d \r\n",chn,channel_num);
    os_printf("********************\r\n");
    #endif
    if(channel_num>0){
        ACT_PRINT("WIFI SET CHANNEL : %d \r\n",channel_cur);
        wifi_set_channel(channel_cur);
        ACT_PRINT("WIFI GET CHANNEL : %d \r\n",wifi_get_channel());
        
        for(i=0;i<channel_num;i++){
            switch_EspnowSendLightCmd(channel_group[i], channelNum, duty, period , code);
        }
    }else{
        switch_EspnowAckCb();//next channel;
    }
}


extern uint32 user_GetBatteryVoltageMv();

void ICACHE_FLASH_ATTR 
switch_EspnowSendLightCmd(uint16 idx, uint16 channelNum, uint32* duty, uint32 period,uint32 code)
{
    os_timer_disarm(&actionReqStatus[idx].req_timer); //disarm retry timer;
    actionReqStatus[idx].sequence+=1 ;//send another seq of cmd
    actionReqStatus[idx].status= ACT_REQ;
    actionReqStatus[idx].retry_num = 0;
    
    EspnowProtoMsg EspnowMsg;
    EspnowMsg.csum = 0;
    EspnowMsg.type = ACT_TYPE_DATA;
    EspnowMsg.token = os_random();
    EspnowMsg.cmd_index=idx;
    EspnowMsg.wifiChannel = wifi_get_channel();
    EspnowMsg.sequence = actionReqStatus[idx].sequence;
    
    EspnowMsg.lightCmd.pwm_num = channelNum;
    EspnowMsg.lightCmd.period = period;
    os_memcpy(EspnowMsg.lightCmd.duty,duty,sizeof(uint32)*channelNum);
    EspnowMsg.lightCmd.cmd_code = code;
    EspnowMsg.lightCmd.batteryStat.battery_voltage_mv=user_GetBatteryVoltageMv();
    if (EspnowMsg.lightCmd.batteryStat.battery_voltage_mv==0) {
        EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_NA;
    } else if (EspnowMsg.lightCmd.batteryStat.battery_voltage_mv<BAT_EMPTY_MV) {
        EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_EMPTY;
    } else {
        EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_OK;
    }
    light_EspnowSetCsum(&EspnowMsg);
    
    //test
    os_printf("***********************\r\n");
    os_printf("EspnowMsg.lightCmd.cmd_code : %d \r\n",EspnowMsg.lightCmd.cmd_code);
    
#if ACT_DEBUG
    ACT_PRINT("send to :\r\n");
    ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
    LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
    int j;
    for(j=0;j<sizeof(EspnowMsg);j++) ACT_PRINT("%02x ",*((uint8*)(&EspnowMsg)+j));
    ACT_PRINT("\r\n");
#endif
    esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&EspnowMsg, sizeof(EspnowProtoMsg));
    
    os_memcpy(  &(actionReqStatus[idx].EspnowMsg), &EspnowMsg, sizeof(EspnowLightCmd));
    //os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
    #if 0
    os_printf("~~~~~~~~~~~~~~~~~~~~~~~~\r\n");
    os_printf("actionReqStatus[%d].sequence : %d \r\n",idx,actionReqStatus[idx].sequence);
    os_printf("actionReqStatus[%d].sequence : %d \r\n",idx,actionReqStatus[idx].status);
    os_printf("actionReqStatus[%d].retry_num : %d \r\n",idx,actionReqStatus[idx].retry_num);
    os_printf("~~~~~~~~~~~~~~~~~~~~~~~~\r\n");
    #endif
}



void ICACHE_FLASH_ATTR 
switch_EspnowSendChnSync(uint8 channel)
{
    ACT_PRINT("SYNC AT CHANNEL %d \r\n",channel);
    wifi_set_channel(channel);
    //ACT_PRINT("TEST SIZEOF actionReqStatus: %d \r\n",sizeof(actionReqStatus));
    int idx;
    bool skip_flg = true;
    for(idx=0;idx<CMD_NUM;idx++){
        if(actionReqStatus[idx].wifichannel == 0){
            skip_flg = false;
            os_timer_disarm(&actionReqStatus[idx].req_timer); //disarm retry timer;
            actionReqStatus[idx].sequence+=1 ;//send another seq of cmd
            actionReqStatus[idx].status= ACT_REQ;
            actionReqStatus[idx].retry_num = 0;
            
            EspnowProtoMsg EspnowMsg;
            EspnowMsg.csum = 0;
            EspnowMsg.type = ACT_TYPE_SYNC;
            EspnowMsg.token= os_random();
            EspnowMsg.cmd_index = idx;
            EspnowMsg.wifiChannel = wifi_get_channel();
            EspnowMsg.sequence = actionReqStatus[idx].sequence;
            EspnowMsg.lightSync.switchChannel=wifi_get_channel();
            EspnowMsg.lightSync.lightChannel = 0;
            EspnowMsg.lightSync.batteryStat.battery_voltage_mv=user_GetBatteryVoltageMv();
            if (EspnowMsg.lightCmd.batteryStat.battery_voltage_mv==0) {
                EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_NA;
            } else if (EspnowMsg.lightCmd.batteryStat.battery_voltage_mv<BAT_EMPTY_MV) {
                EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_EMPTY;
            } else {
                EspnowMsg.lightCmd.batteryStat.battery_status=ACT_BAT_OK;
            }
            light_EspnowSetCsum(&EspnowMsg);
            
        #if ACT_DEBUG
            ACT_PRINT("send to :\r\n");
            ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
            LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
            int j;
            for(j=0;j<sizeof(EspnowProtoMsg);j++) ACT_PRINT("%02x ",*((uint8*)(&EspnowMsg)+j));
            ACT_PRINT("\r\n");
        #endif
            os_memcpy(  &(actionReqStatus[idx].EspnowMsg), &EspnowMsg, sizeof(EspnowProtoMsg));
            esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&EspnowMsg, sizeof(EspnowProtoMsg));
            //os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
        }
    }
    if(skip_flg){
        switch_EspnowSyncCb();
    }
}


void ICACHE_FLASH_ATTR
switch_EspnowChnSyncStart()
{
    int i;
    for(i=0;i<LIGHT_DEV_NUM;i++){
        actionReqStatus[i].wifichannel = 0;
    }
    channel_cur = 1;
    switch_EspnowSendChnSync(channel_cur);
}

void ICACHE_FLASH_ATTR 
switch_EspnowRcvCb(u8 *macaddr, u8 *data, u8 len)
{
    int i;
#if ACT_DEBUG
    ACT_PRINT("recv mac : \r\n");
    for(i = 0; i<6;i++){
        ACT_PRINT("%02x ",macaddr[i]);
    }
    ACT_PRINT("\r\n");
    ACT_PRINT("recv data: ");
    for (i = 0; i < len; i++)
    ACT_PRINT("%02X, ", data[i]);
    ACT_PRINT("\n");
#endif
    
    EspnowProtoMsg EspnowMsg;
    os_memcpy( (uint8*)(&EspnowMsg),data,len);
    if(light_EspnowCmdValidate(&EspnowMsg) ){
        ACT_PRINT("cmd check sum OK\r\n");
        uint32 _idx=EspnowMsg.cmd_index;
        if(0 == os_memcmp(macaddr+1, LIGHT_MAC[_idx]+1,sizeof(SWITCH_MAC)-1)){
            ACT_PRINT("switch MAC match...\r\n");
            if(EspnowMsg.sequence == actionReqStatus[_idx].sequence && EspnowMsg.type == ACT_TYPE_RSP ){  
            //ACT WOULD NOT HAPPEN AFTER WE USE MAC LAYER ACK
            actionReqStatus[_idx].status = ACT_RSP;
            ACT_PRINT("CMD RESPONSE %d  \r\n",_idx);
            ACT_PRINT("DO NOTHING...\r\n");
            ACT_PRINT("CMD CHANNEL : %d \r\n",EspnowMsg.wifiChannel);
            ACT_PRINT("SELF CHANNEL: %d \r\n",wifi_get_channel());
            }else if(EspnowMsg.sequence == actionReqStatus[_idx].sequence && EspnowMsg.type == ACT_TYPE_SYNC){
                actionReqStatus[_idx].status = ACT_RSP;
                if(wifi_get_channel()==EspnowMsg.wifiChannel){
                    actionReqStatus[_idx].wifichannel = EspnowMsg.wifiChannel;
                    ACT_PRINT("cmd %d sync,@ CHANNEL %d \r\n",_idx,actionReqStatus[_idx].wifichannel);
                    os_timer_disarm(&actionReqStatus[_idx].req_timer);
                    switch_EspnowSyncCb();
                }else{
                    ACT_PRINT("MESH SYNC CHANNEL ERROR, get channel : %d , data_channel : %d\r\n",wifi_get_channel(),EspnowMsg.wifiChannel);
                }
            }else{
                if(EspnowMsg.sequence != actionReqStatus[_idx].sequence) ACT_PRINT("seq error\r\n");
                if(EspnowMsg.type != ACT_TYPE_RSP ) ACT_PRINT("TYPE MISMATCH: %d \r\n",EspnowMsg.type);
            }
        
        }else{
            ACT_PRINT("SOURCE MAC: "MACSTR,MAC2STR(macaddr));
            ACT_PRINT("LIGHT RECORD MAC: "MACSTR,MAC2STR(LIGHT_MAC[_idx]));
            ACT_PRINT("LIGHT IDX: %d \r\n",EspnowMsg.cmd_index); 
            ACT_PRINT("switch MAC mismatch...\r\n");
        }
    }else{
        ACT_PRINT("cmd check sum error\r\n");
    }

}



int ICACHE_FLASH_ATTR
switch_GetLightMacIdx(uint8* mac_addr)
{
    int i;
    uint8 m_l = *(mac_addr+DEV_MAC_LEN-1);
    for(i=0;i<LIGHT_DEV_NUM;i++){
        if( m_l== LIGHT_MAC[i][DEV_MAC_LEN-1]){
            if(0==os_memcmp(mac_addr,LIGHT_MAC[i],DEV_MAC_LEN)){
                return i;
            }
        }
    }
    return -1;
}


void ICACHE_FLASH_ATTR
	esp_now_send_cb(u8 *mac_addr, u8 status)
{
    os_printf("====================\r\n");
    os_printf("ESP-NOW SEND CB\r\n");
    os_printf("--------\r\n");
    os_printf("MAC: "MACSTR"\r\n",MAC2STR(mac_addr));
    os_printf("STATUS: %d \r\n",status);
    os_printf("====================\r\n");

    int mac_idx = switch_GetLightMacIdx(mac_addr) ;
    if(mac_idx < 0 && mac_idx>=CMD_NUM){
        os_printf("MAC idx error: %d \r\n",mac_idx);
        return;
    }

	EspnowProtoMsg* EspnowRetryMsg = &(actionReqStatus[mac_idx].EspnowMsg);
	Action_SendStatus* EspnowSendStatus = &(actionReqStatus[mac_idx]);

    if(status==0){ //send successful
        EspnowSendStatus->status = ACT_ACK;//ACT NOT RESPONSED YET,FOR CMD ACK IS ENOUGH, FOR SYNC , WAITING FOR RESPONSE
        os_printf("data ACKed...\r\n");
        os_timer_disarm(&EspnowSendStatus->req_timer);
        if(EspnowRetryMsg->type==ACT_TYPE_DATA){
            switch_EspnowAckCb();
        }else if(EspnowRetryMsg->type==ACT_TYPE_SYNC){
            os_printf("go to set retry:\r\n");
            goto SET_RETRY;
        }
    
    }else{ //send fail
SET_RETRY:
    os_timer_disarm(&EspnowSendStatus->req_timer);
    if(EspnowSendStatus->retry_num < ACTION_RETRY_NUM){
        os_printf("data[%d] send failed...retry:%d\r\n",mac_idx,EspnowSendStatus->retry_num);
        os_timer_arm( &EspnowSendStatus->req_timer, EspnowSendStatus->retry_expire,0);
    }else{
        ACT_PRINT("retry num exceed..stop retry, type: %d\r\n",EspnowRetryMsg->type);
        EspnowSendStatus->status = ACT_TIME_OUT;        
        if(EspnowRetryMsg->type==ACT_TYPE_SYNC){
            switch_EspnowSyncCb();
        }else if(EspnowRetryMsg->type==ACT_TYPE_DATA){
            switch_EspnowAckCb();
        }
    }
    }
}

void ICACHE_FLASH_ATTR switch_EspnowInit()
{
    uint8 i;
    int e_res;
    if (esp_now_init()==0) {
        os_printf("direct link  init ok\n");
        esp_now_register_recv_cb(switch_EspnowRcvCb);
    } else {
        os_printf("dl init failed\n");
    }
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);  //role 1: switch   ;  role 2 : light;
#if ESPNOW_KEY_HASH
    esp_now_set_kok(esp_now_key, ESPNOW_KEY_LEN);
#endif
    esp_now_register_send_cb(esp_now_send_cb);
        system_param_load(FLASH_PARAM_SEC,0,&flashData_t,sizeof(flashData_t));
    ACT_PRINT("MAGIC: %08x\r\n",flashData_t.magic);
    for(i=0;i<CMD_NUM;i++){
        
    #if ESPNOW_ENCRYPT
        e_res = esp_now_add_peer((uint8*)LIGHT_MAC[i], (uint8)ESP_NOW_ROLE_SLAVE,(uint8)WIFI_DEFAULT_CHANNEL, esp_now_key, (uint8)ESPNOW_KEY_LEN);//wjl
        if(e_res){
            os_printf("ADD PEER ERROR!!!!!MAX NUM!!!!!\r\n");
            return;
        }
    #else
        esp_now_add_peer((uint8*)LIGHT_MAC[i], (uint8)ESP_NOW_ROLE_SLAVE,(uint8)WIFI_DEFAULT_CHANNEL, NULL, (uint8)ESPNOW_KEY_LEN);//wjl
    #endif
        actionReqStatus[i].actionToutCb = (ActionToutCallback)switch_EspnowSendRetry;
        
        os_memset(&(actionReqStatus[i].EspnowMsg),0,sizeof(EspnowProtoMsg));
        os_timer_disarm(&actionReqStatus[i].req_timer);
        os_timer_setfn(&actionReqStatus[i].req_timer,  actionReqStatus[i].actionToutCb , &(actionReqStatus[i].EspnowMsg.cmd_index)  );
        actionReqStatus[i].wifichannel = ( (flashData_t.magic==FLASH_DATA_MAGIC)?flashData_t.wifiChannel[i]:WIFI_DEFAULT_CHANNEL);
        ACT_PRINT("LIGHT %d : channel %d \r\n",i,actionReqStatus[i].wifichannel);
        actionReqStatus[i].retry_num=0;
        actionReqStatus[i].sequence =0;
        actionReqStatus[i].status = ACT_IDLE;
        actionReqStatus[i].retry_expire = ACTION_RETRY_TIMER_MS;  //300ms retry
    }
}

void ICACHE_FLASH_ATTR switch_EspnowDeinit()
{
    esp_now_unregister_recv_cb(); 
    esp_now_deinit();
}

#endif

