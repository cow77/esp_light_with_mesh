#include "os_type.h"
#include "eagle_soc.h"
#include "c_types.h"
#include "osapi.h"
#include "ets_sys.h"
#include "mem.h"
#include "user_light_action.h"
#include "user_light_adj.h"
#include "user_light.h"
#include "user_interface.h"
#include "espnow.h"
#include "user_esp_platform.h"

#if ESP_NOW_SUPPORT
//The espnow packet struct used for light<->switch intercommunications
typedef struct{
    uint16 battery_voltage_mv;
    uint16 battery_status;
}EspnowBatStatus;

typedef enum{
    COLOR_SET = 0,
    COLOR_CHG ,
    COLOR_TOGGLE,
    COLOR_LEVEL,
    LIGHT_RESET
}EspnowCmdCode;


typedef struct {
	uint16 pwm_num;
	uint32 period;
	uint32 duty[8];
	uint32 cmd_code;
    EspnowBatStatus batteryStat;
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
    ACT_RSP =4,
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


#define ACT_DEBUG 1
#define ACT_PRINT os_printf
#define WIFI_DEFAULT_CHANNEL 1

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

typedef struct {
	char mac[6];
	int status;
	int battery_mv;
} SwitchBatteryStatus;

//Just save the state of one switch for now.
SwitchBatteryStatus switchBattery;


static void ICACHE_FLASH_ATTR light_EspnowStoreBatteryStatus(char *mac, int status, int voltage_mv) {
    os_memcpy(switchBattery.mac, mac, 6);
    switchBattery.status=status;
    switchBattery.battery_mv=voltage_mv;
}

//ToDo: add support for more switches
int ICACHE_FLASH_ATTR light_EspnowGetBatteryStatus(int idx, char *mac, int *status, int *voltage_mv) {
    if (idx>0) return 0;
    os_memcpy(mac, switchBattery.mac, 6);
    *status=switchBattery.status;
    *voltage_mv=switchBattery.battery_mv;
}


#if LIGHT_DEVICE
#define LIGHT_DEBUG 1
void ICACHE_FLASH_ATTR light_EspnowRcvCb(u8 *macaddr, u8 *data, u8 len)
{
    int i;
    uint32 duty_set[PWM_CHANNEL] = {0};
    uint32 period=1000;
    LOCAL uint8 color_bit=1;
    LOCAL uint8 toggle_flg = 0;
    
    EspnowProtoMsg EspnowMsg;
    os_memcpy((uint8*)(&EspnowMsg), data,sizeof(EspnowProtoMsg));
    ACT_PRINT("RCV DATA: \r\n");
    for(i=0;i<len;i++) ACT_PRINT("%02x ",data[i]);
    ACT_PRINT("\r\n-----------------------\r\n");
    
    
    
    if(light_EspnowCmdValidate(&EspnowMsg) ){
        ACT_PRINT("cmd check sum ok\r\n");
        #if (LIGHT_DEBUG==0)  
        //ACTUALL WE CAN GET THE PRACTIACAL MAC OF THE SOURCE NOW: (*macaddr)
        if (0 != os_memcmp( macaddr, SWITCH_MAC,sizeof(SWITCH_MAC))){
			ACT_PRINT("MAC CHECK ERROR...RETURN\r\n");
            return;
        }
        #endif
        //if(light_cmd.wifiChannel == wifi_get_channel()){
            //ACT_TYPE_DATA: this is a set of data or command.
            ACT_PRINT("ESPNOW CMD CODE : %d \r\n",EspnowMsg.lightCmd.cmd_code);
            if(EspnowMsg.type == ACT_TYPE_DATA){
                ACT_PRINT("period: %d ; channel : %d \r\n",EspnowMsg.lightCmd.period,EspnowMsg.lightCmd.pwm_num);
                for(i=0;i<EspnowMsg.lightCmd.pwm_num;i++){
                    ACT_PRINT(" duty[%d] : %d \r\n",i,EspnowMsg.lightCmd.duty[i]);
                    duty_set[i] = EspnowMsg.lightCmd.duty[i];
                }
                
                ACT_PRINT("SOURCE MAC CHEK OK \r\n");
                ACT_PRINT("cmd channel : %d \r\n",EspnowMsg.wifiChannel);
                ACT_PRINT("SELF CHANNEL: %d \r\n",wifi_get_channel());
                ACT_PRINT("Battery status %d voltage %dmV\r\n", EspnowMsg.lightCmd.batteryStat.battery_status, EspnowMsg.lightCmd.batteryStat.battery_voltage_mv);
                light_EspnowStoreBatteryStatus(macaddr, EspnowMsg.lightCmd.batteryStat.battery_status, EspnowMsg.lightCmd.batteryStat.battery_voltage_mv);
                
                os_printf("***************\r\n");
                os_printf("EspnowMsg.lightCmd.colorChg: %d \r\n",EspnowMsg.lightCmd.cmd_code);
                if(EspnowMsg.lightCmd.cmd_code==COLOR_SET){
                    color_bit = 1;
                    toggle_flg= 0;
                    os_printf("set color : %d %d %d %d %d %d \r\n",duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],EspnowMsg.lightCmd.period);
                    light_set_aim(duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],EspnowMsg.lightCmd.period,0);
                }else if(EspnowMsg.lightCmd.cmd_code==COLOR_CHG){
                    os_printf("light change color cmd \r\n");
                    for(i=0;i<PWM_CHANNEL;i++){
                        duty_set[i] = ((color_bit>>i)&0x1)*20000;
                    }
                    os_printf("set color : %d %d %d %d %d %d\r\n",duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],EspnowMsg.lightCmd.period);
                    light_set_aim( duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],1000,0);	
                    color_bit++;
                    if(color_bit>=32) color_bit=1;
                }else if(EspnowMsg.lightCmd.cmd_code==COLOR_TOGGLE){
                    if(toggle_flg == 0){
                        light_set_aim( 0,0,0,0,0,1000,0); 
                        toggle_flg = 1;
                    }else{
                        light_set_aim( 0,0,0,20000,20000,1000,0); 
                        toggle_flg = 0;
                    }
                }else if(EspnowMsg.lightCmd.cmd_code==COLOR_LEVEL){
                    struct ip_info sta_ip;
                    wifi_get_ip_info(STATION_IF,&sta_ip);
                #if ESP_MESH_SUPPORT						
                    if( espconn_mesh_local_addr(&sta_ip.ip)){
                        os_printf("THIS IS A MESH SUB NODE..\r\n");
                        uint32 mlevel = sta_ip.ip.addr&0xff;
                        light_ShowDevLevel(mlevel);
                    }else if(sta_ip.ip.addr!= 0){
                        os_printf("THIS IS A MESH ROOT..\r\n");
                        light_ShowDevLevel(1);
                    }else{
                        os_printf("THIS IS A MESH ROOT..\r\n");
                        light_ShowDevLevel(0);
                    }
                #else
                    if(sta_ip.ip.addr!= 0){
                        os_printf("wifi connected..\r\n");
                        light_ShowDevLevel(1);
                    }else{
                        os_printf("wifi not connected..\r\n");
                        light_ShowDevLevel(0);
                    }
                #endif
                    
                
                }else if(EspnowMsg.lightCmd.cmd_code==LIGHT_RESET){
                system_restore();
                extern struct esp_platform_saved_param esp_param;
                esp_param.reset_flg=0;
                esp_param.activeflag = 0;
                os_memset(esp_param.token,0,40);
                system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
                os_printf("--------------------\r\n");
                os_printf("SYSTEM PARAM RESET !\r\n");
                os_printf("RESET: %d ;  ACTIVE: %d \r\n",esp_param.reset_flg,esp_param.activeflag);
                os_printf("--------------------\r\n\n\n");
                UART_WaitTxFifoEmpty(0,3000);
                system_restart();
                }
                ACT_PRINT("do not send response,in app layer, MAC ACKed \r\n");
                return;
            }
            //ACT_TYPE_SYNC: AFTER LIGHT CONFIGURED TO ROUTER, ITS CHANNEL CHANGES
            //               NEED TO SET THE CHANNEL FOR EACH LIGHT.
            else if(EspnowMsg.type == ACT_TYPE_SYNC && (EspnowMsg.lightSync.switchChannel == wifi_get_channel())){
                EspnowMsg.type = ACT_TYPE_SYNC;
                ACT_PRINT("cmd rcv channel : %d \r\n",EspnowMsg.wifiChannel);
                EspnowMsg.wifiChannel = wifi_get_channel();
                EspnowMsg.lightSync.lightChannel = wifi_get_channel();
                ACT_PRINT("send sync, self channel : %d  \r\n",wifi_get_channel());
                light_Espnow_ShowSyncSuc();
            }else{
                ACT_PRINT(" data type %d \r\n",EspnowMsg.type);
                ACT_PRINT("data channel :%d \r\n",EspnowMsg.wifiChannel);
                ACT_PRINT("data self channel: %d \r\n",wifi_get_channel());
                ACT_PRINT("data type or channel error\r\n");
                return;
            }
            light_EspnowSetCsum(&EspnowMsg);
            
        #if ACT_DEBUG
            int j;
            for(j=0;j<sizeof(EspnowLightCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&EspnowMsg)+j));
            ACT_PRINT("\r\n");
        #endif
        #if (LIGHT_DEBUG==0)
            ACT_PRINT("send to mac: %02x %02x %02x %02x %02x %02x\r\n",
            SWITCH_MAC[0],SWITCH_MAC[1],SWITCH_MAC[2],SWITCH_MAC[3],SWITCH_MAC[4],SWITCH_MAC[5]);
            esp_now_send(SWITCH_MAC, (uint8*)(&EspnowMsg), sizeof(EspnowProtoMsg)); //send ack
        #else
            esp_now_send(macaddr, (uint8*)(&EspnowMsg), sizeof(EspnowProtoMsg)); //send ack
        #endif
        //}
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
    #if ESPNOW_KEY_HASH
    esp_now_set_kok(esp_now_key, ESPNOW_KEY_LEN);
    #endif
    
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

#define ACTION_RETRY_NUM  3
#define ACTION_RETRY_TIMER_MS  500
typedef void (*ActionToutCallback)(uint32* act_idx);

typedef struct{
    uint32 sequence;
    EspnowMsgStatus status;
    os_timer_t req_timer;
    uint32 retry_num;
    uint32 retry_expire;
    ActionToutCallback actionToutCb;
    EspnowLightCmd lightActionCmd;
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

bool ICACHE_FLASH_ATTR
	switch_CheckCmdResult()
{
    int i;
	for(i=0;i<channel_num;i++){
        if(actionReqStatus[channel_group[i]].status == ACT_REQ) return false;
	}
	return true;
}


void ICACHE_FLASH_ATTR
	switch_EspnowAckCb()
{
	if( switch_CheckCmdResult() ){
		if(channel_cur == 14){
		    ACT_PRINT("release power\r\n");
		    _SWITCH_GPIO_RELEASE();
		}else{
			ACT_PRINT("SEND NEXT CHANNEL: %d \r\n",++channel_cur);
			switch_EspnowSendCmdByChnl(channel_cur,pwm_chn_num, pwm_duty, pwm_period);
		}
	}

}

bool ICACHE_FLASH_ATTR
	switch_CheckSyncResult()
{
    int i;
	for(i=0;i<CMD_NUM;i++){ //CMD NUM QUEALS PRACTICAL LIGHT NUMBER
        if(actionReqStatus[i].status == ACT_REQ ) return false;
	}
	return true;
}


void ICACHE_FLASH_ATTR
	switch_EspnowSyncCb()
{
	if( switch_CheckSyncResult() ){
		os_printf("SYNC FINISHED ...\r\n");
		#if 0
		UART_WaitTxFifoEmpty(0,100000);
		_SWITCH_GPIO_RELEASE();
		#else
		if(channel_cur == 14){
		    ACT_PRINT("release power\r\n");

			int i;
			for(i=0;i<CMD_NUM;i++){
				flashData_t.magic = FLASH_DATA_MAGIC;
				flashData_t.wifiChannel[i] = actionReqStatus[i].wifichannel;
			}
			system_param_save_with_protect(FLASH_PARAM_SEC,&flashData_t,sizeof(flashData_t));
			//system_param_load(FLASH_PARAM_SEC,0,&flashData_t,sizeof(flashData_t));
			
		    _SWITCH_GPIO_RELEASE();
		}else{
			ACT_PRINT("SYNC NEXT CHANNEL: %d \r\n",++channel_cur);
			//switch_EspnowSendCmdByChnl(channel_cur,pwm_chn_num, pwm_duty, pwm_period);
			switch_EspnowSendChnSync(channel_cur);
		}
		#endif

	}

}




 void ICACHE_FLASH_ATTR
 	switch_EspnowSendRetry(void* arg)
{
    uint32 _idx = *((uint32*)arg);
    EspnowLightCmd* action_retry_cmd = &(actionReqStatus[_idx].lightActionCmd);
    Action_SendStatus* action_status = &(actionReqStatus[_idx]);
    
    if((action_retry_cmd->sequence == action_status->sequence)){
        if(action_status->status== ACT_REQ){
            esp_now_send((uint8*)LIGHT_MAC[_idx], (uint8*)action_retry_cmd, sizeof(EspnowLightCmd));
            action_status->retry_num++;
			if(action_status->retry_num < ACTION_RETRY_NUM){
                os_timer_arm( &action_status->req_timer, action_status->retry_expire,0);
			}else{
                ACT_PRINT("retry num exceed..stop retry, cmd type: %d\r\n",action_retry_cmd->type);
				action_status->status = ACT_TIME_OUT;
				
				if(action_retry_cmd->type==ACT_TYPE_SYNC){
					switch_EspnowSyncCb();
				}else if(action_retry_cmd->type==ACT_TYPE_DATA){
				    switch_EspnowAckCb();
				}
			}
    	}else{
            ACT_PRINT("STATUS error : %d\r\n",action_status->status);
    	}
        
    }else if(action_retry_cmd->sequence   <   action_status->sequence){
        ACT_PRINT("action updated...,cancel retry ...\r\n");
    }


}

void ICACHE_FLASH_ATTR switch_EspnowSendCmdByChnl(uint8 chn,uint32 channelNum, uint32* duty, uint32 period)
{
	int i = 0;
	os_memset(channel_group, 0 ,sizeof(channel_group));
	channel_num = 0;
	channel_cur = chn;
	
	pwm_period = period;
	os_memcpy(pwm_duty,duty,sizeof(pwm_duty));
	pwm_chn_num = channelNum;
	
	
	for(i=0;i<CMD_NUM;i++){
		if(actionReqStatus[i].wifichannel == chn){
			channel_group[channel_num++]=i;
			ACT_PRINT("CHANNEL %d : add idx %d\r\n",chn,i);
		}
	}

	if(channel_num>0){
		ACT_PRINT("WIFI SET CHANNEL : %d \r\n",channel_cur);
		wifi_set_channel(channel_cur);
		ACT_PRINT("WIFI GET CHANNEL : %d \r\n",wifi_get_channel());

		for(i=0;i<channel_num;i++){
			switch_EspnowSendLightCmd(channel_group[i], channelNum, duty, period);
			
		}
	}else{
		switch_EspnowAckCb();//next channel;
    }
}


extern uint32 user_GetBatteryVoltageMv();

void ICACHE_FLASH_ATTR switch_EspnowSendLightCmd(uint8 idx, uint32 channelNum, uint32* duty, uint32 period)
{
    os_timer_disarm(&actionReqStatus[idx].req_timer); //disarm retry timer;
    actionReqStatus[idx].sequence+=1 ;//send another seq of cmd
    actionReqStatus[idx].status= ACT_REQ;
    actionReqStatus[idx].lightActionCmd.cmd_index = idx;
    actionReqStatus[idx].retry_num = 0;
    
    EspnowLightCmd light_cmd;
    light_cmd.cmd_index = idx;
    light_cmd.sequence = actionReqStatus[idx].sequence; //send another seq of cmd
    light_cmd.type = ACT_TYPE_DATA;
    light_cmd.pwm_num = channelNum;
	light_cmd.wifiChannel = wifi_get_channel();
	light_cmd.battery_voltage_mv=user_GetBatteryVoltageMv();
	if (light_cmd.battery_voltage_mv==0) {
		light_cmd.battery_status=ACT_BAT_NA;
	} else if (light_cmd.battery_voltage_mv<BAT_EMPTY_MV) {
		light_cmd.battery_status=ACT_BAT_EMPTY;
	} else {
		light_cmd.battery_status=ACT_BAT_OK;
	}
	os_memcpy(light_cmd.duty,duty,sizeof(uint32)*channelNum);
	uint8 mac_buf[6] = {0};
	wifi_get_macaddr(STATION_IF,mac_buf);
	os_printf("source mac: %02x %02x %02x %02x %02x %02x\r\n",mac_buf[0],mac_buf[1],mac_buf[2],mac_buf[3],mac_buf[4],mac_buf[5]);
    os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));

    light_cmd.period = period;
    light_EspnowSetCsum(&light_cmd);

    #if ACT_DEBUG
	ACT_PRINT("send to :\r\n");
	ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
		                                               LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
	int j;
    for(j=0;j<sizeof(EspnowLightCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
	ACT_PRINT("\r\n");
    #endif
    esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&light_cmd, sizeof(EspnowLightCmd));

    os_memcpy(  &(actionReqStatus[idx].lightActionCmd), &light_cmd, sizeof(EspnowLightCmd));
    os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
	
}

void ICACHE_FLASH_ATTR 
	switch_EspnowSendChnSync(uint8 channel)
{
	ACT_PRINT("SYNC AT CHANNEL %d \r\n",channel);
	wifi_set_channel(channel);
    ACT_PRINT("TEST SIZEOF actionReqStatus: %d \r\n",sizeof(actionReqStatus));
	int idx;
	bool skip_flg = true;
	for(idx=0;idx<CMD_NUM;idx++){
    	if(actionReqStatus[idx].wifichannel == 0){
			skip_flg = false;
            os_timer_disarm(&actionReqStatus[idx].req_timer); //disarm retry timer;
            actionReqStatus[idx].sequence+=1 ;//send another seq of cmd
            actionReqStatus[idx].status= ACT_REQ;
            actionReqStatus[idx].lightActionCmd.cmd_index = idx;
            actionReqStatus[idx].retry_num = 0;
        
            EspnowLightCmd light_cmd;
            light_cmd.cmd_index = idx;
            light_cmd.sequence = actionReqStatus[idx].sequence; //send another seq of cmd
            light_cmd.type = ACT_TYPE_SYNC;
        	light_cmd.wifiChannel= wifi_get_channel();
			ACT_PRINT("CMD SEND CHANNLE: %d \r\n",light_cmd.wifiChannel);
       	
        	uint8 mac_buf[6] = {0};
        	wifi_get_macaddr(STATION_IF,mac_buf);
        	os_printf("source mac: %02x %02x %02x %02x %02x %02x\r\n",mac_buf[0],mac_buf[1],mac_buf[2],mac_buf[3],mac_buf[4],mac_buf[5]);
            os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
            light_EspnowSetCsum(&light_cmd);
        
            #if ACT_DEBUG
        	ACT_PRINT("send to :\r\n");
        	ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
        		                                               LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
        	int j;
            for(j=0;j<sizeof(EspnowLightCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
        	ACT_PRINT("\r\n");
            #endif
            esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&light_cmd, sizeof(EspnowLightCmd));
            os_memcpy(  &(actionReqStatus[idx].lightActionCmd), &light_cmd, sizeof(EspnowLightCmd));
            os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
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

void ICACHE_FLASH_ATTR switch_EspnowRcvCb(u8 *macaddr, u8 *data, u8 len)
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

    EspnowLightCmd light_data ;
    os_memcpy( (uint8*)(&light_data),data, len);
    uint32 light_index=light_data.cmd_index;


	if(light_EspnowCmdValidate(&light_data) ){
		ACT_PRINT("cmd check sum OK\r\n");

       if(0 == os_memcmp(light_data.source_mac+1, LIGHT_MAC[light_index]+1,sizeof(SWITCH_MAC)-1)){
               ACT_PRINT("switch MAC match...\r\n");
       	uint32 _idx = light_data.cmd_index;
       	if(light_data.sequence == actionReqStatus[_idx].sequence && light_data.type == ACT_TYPE_RSP ){
       		actionReqStatus[_idx].status = ACT_RSP;
       		ACT_PRINT("cmd %d ack \r\n",_idx);
			ACT_PRINT("cmd channel : %d \r\n",light_data.wifiChannel);
			ACT_PRINT("SELF CHANNEL: %d \r\n",wifi_get_channel());
			switch_EspnowAckCb();
       	}else if(light_data.sequence == actionReqStatus[_idx].sequence && light_data.type == ACT_TYPE_SYNC){
			actionReqStatus[_idx].status = ACT_RSP;
			if(wifi_get_channel()==light_data.wifiChannel){
			    actionReqStatus[_idx].wifichannel = light_data.wifiChannel;
       		    ACT_PRINT("cmd %d sync,@ CHANNEL %d \r\n",_idx,actionReqStatus[_idx].wifichannel);
			    switch_EspnowSyncCb();
			}else{
				ACT_PRINT("MESH SYNC CHANNEL ERROR, get channel : %d , data_channel : %d\r\n",wifi_get_channel,light_data.wifiChannel);
			}
		}else{
            if(light_data.sequence != actionReqStatus[_idx].sequence) ACT_PRINT("seq error\r\n");
       	    if(light_data.type != ACT_TYPE_RSP ) ACT_PRINT("TYPE MISMATCH: %d \r\n",light_data.type);
       	}
       
           }else{
				ACT_PRINT("SOURCE MAC: "MACSTR,MAC2STR(light_data.source_mac));
				ACT_PRINT("LIGHT RECORD MAC: "MACSTR,MAC2STR(LIGHT_MAC[light_index]));
				ACT_PRINT("LIGHT IDX: %d \r\n",light_data.cmd_index); 
			   ACT_PRINT("switch MAC mismatch...\r\n");
        	}
	}else{
		ACT_PRINT("cmd check sum error\r\n");
	}

}

void ICACHE_FLASH_ATTR switch_EspnowInit()
{
    uint8 i;
	int e_res;

	
	if (esp_now_init()==0) {
		os_printf("direct link  init ok\n");
		switch_EspnowRcvCb(switch_action_rcv_cb);
	} else {
		os_printf("dl init failed\n");
	}
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);  //role 1: switch   ;  role 2 : light;
	esp_now_set_kok(esp_now_key, ESPNOW_KEY_LEN);


	system_param_load(FLASH_PARAM_SEC,0,&flashData_t,sizeof(flashData_t));
	
    ACT_PRINT("MAGIC: %08x\r\n",flashData_t.magic);
    for(i=0;i<CMD_NUM;i++){
		
		#if ESPNOW_ENCRYPT
    		e_res = esp_now_add_peer((uint8*)LIGHT_MAC[i], (uint8)ESP_NOW_ROLE_SLAVE,(uint8)WIFI_DEFAULT_CHANNEL, esp_now_key, (uint8)ESPNOW_KEY_LEN);//wjl
			if(e_res){
				os_printf("ERROR!!!!!!!!!!\r\n");
				return;
			}
		#else
    		esp_now_add_peer((uint8*)LIGHT_MAC[i], (uint8)ESP_NOW_ROLE_SLAVE,(uint8)WIFI_DEFAULT_CHANNEL, NULL, (uint8)ESPNOW_KEY_LEN);//wjl
		#endif
        actionReqStatus[i].actionToutCb = (ActionToutCallback)switch_EspnowSendRetry;
        
        os_memset(&(actionReqStatus[i].lightActionCmd),0,sizeof(EspnowLightCmd));
        os_timer_disarm(&actionReqStatus[i].req_timer);
        os_timer_setfn(&actionReqStatus[i].req_timer,  actionReqStatus[i].actionToutCb , &(actionReqStatus[i].lightActionCmd.cmd_index)  );
        
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








#endif




