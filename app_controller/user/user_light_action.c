
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
typedef struct {
	uint8 source_mac[6];
	uint16 wifiChannel;
	uint32 cmd_index;
	uint32 sequence;
	uint16 type;
	uint16 pwm_num;
	uint32 period;
	uint32 duty[8];
	uint16 battery_voltage_mv;
	uint16 battery_status;
	uint32 check_sum;
} LightActionCmd;

typedef enum{
    ACT_IDLE = 0,
    ACT_ACK = 1,
    ACT_REQ = 2,
    ACT_TIME_OUT =3,
}ActionStatus;

typedef enum{
    ACT_DATA = 0,
    ACT_TYPE_ACK = 1,
    ACT_TYPE_SYNC = 2,
}ACTION_TYPE;


typedef enum {
	ACT_BAT_NA = 0,
	ACT_BAT_OK = 1,
	ACT_BAT_EMPTY = 2
}ACT_BAT_TYPE;

#define BAT_EMPTY_MV 2100

#define ACT_DEBUG 1
#define ACT_PRINT os_printf
#define WIFI_DEFAULT_CHANNEL 1


bool ICACHE_FLASH_ATTR
	light_action_cmd_validate(LightActionCmd* light_act_data)
{
    uint8* data = (uint8*)light_act_data;
    uint32 csum_cal = 0;
    int i;
    for(i=0;i<(sizeof(LightActionCmd)-4);i++){
		csum_cal+= *(data+i);
    }
    if(csum_cal == light_act_data->check_sum){
        return true;
    }else{
        return false;
    }
}

void ICACHE_FLASH_ATTR
	light_action_set_csum(LightActionCmd* light_act_data)
{
    uint8* data = (uint8*)light_act_data;
    uint32 csum_cal = 0;
    int i;
    for(i=0;i<(sizeof(LightActionCmd)-4);i++){
		csum_cal+= *(data+i);
    }
    light_act_data->check_sum = csum_cal;
}


#if LIGHT_DEVICE

#define LIGHT_DEBUG 1
void ICACHE_FLASH_ATTR light_action_rcv_cb(u8 *macaddr, u8 *data, u8 len)
{
	int i;
	uint32 duty_set[PWM_CHANNEL] = {0};
	uint32 period=1000;
	   
    LightActionCmd light_cmd ;
    os_memcpy((uint8*)(&light_cmd), data,sizeof(LightActionCmd));
		


	if(light_action_cmd_validate(&light_cmd) ){
		ACT_PRINT("cmd check sum ok\r\n");
		#if (LIGHT_DEBUG==0)  
       	if (0 == os_memcmp( light_cmd.source_mac, SWITCH_MAC,sizeof(SWITCH_MAC))){
		#else  //debug!!!
       	if(true){ //debug
       	
		    uint8 mac_bkp[6] = {0};
		    os_memcpy(mac_bkp,light_cmd.source_mac,sizeof(mac_bkp));
			uint8 mac_buf[6] = {0};
			wifi_get_macaddr(STATION_IF,mac_buf);
       	#endif

		    //if(light_cmd.wifiChannel == wifi_get_channel()){
				if(light_cmd.type == ACT_DATA){
					ACT_PRINT("period: %d ; channel : %d \r\n",light_cmd.period,light_cmd.pwm_num);
					for(i=0;i<light_cmd.pwm_num;i++){
							ACT_PRINT(" duty[%d] : %d \r\n",i,light_cmd.duty[i]);
						duty_set[i] = light_cmd.duty[i];
					}
			       ACT_PRINT("SOURCE MAC CHEK OK \r\n");
               	   light_set_aim(duty_set[0],duty_set[1],duty_set[2],duty_set[3],duty_set[4],light_cmd.period,0);
                   light_cmd.type = ACT_TYPE_ACK;
				   os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
                   os_printf("send ack \r\n");
				}
				else if(light_cmd.type == ACT_TYPE_SYNC && (light_cmd.wifiChannel == wifi_get_channel())){
					light_cmd.type = ACT_TYPE_SYNC;
					os_printf("cmd rcv channel : %d \r\n",light_cmd.wifiChannel);
					light_cmd.wifiChannel = wifi_get_channel();
					os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
					//os_memcpy(light_cmd.source_mac,LIGHT_MAC[light_cmd.cmd_index],sizeof(LIGHT_MAC[light_cmd.cmd_index]));
					os_printf("send sync, self channel : %d  \r\n",wifi_get_channel());
				}else{
					os_printf(" data type %d \r\n",light_cmd.type);
					os_printf("data channel :%d \r\n",light_cmd.wifiChannel);
					os_printf("data self channel: %d \r\n",wifi_get_channel());
					os_printf("data type or channel error\r\n");
					return;
				}
		     	light_action_set_csum(&light_cmd);
				   #if ACT_DEBUG
					  int j;
				   for(j=0;j<sizeof(LightActionCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
				   ACT_PRINT("\r\n");
				   #endif
				   #if (LIGHT_DEBUG==0)
				   
                       ACT_PRINT("send to mac: %02x %02x %02x %02x %02x %02x\r\n",
    				   	SWITCH_MAC[0],SWITCH_MAC[1],SWITCH_MAC[2],SWITCH_MAC[3],SWITCH_MAC[4],SWITCH_MAC[5]);
                       esp_now_send(SWITCH_MAC, (uint8*)(&light_cmd), sizeof(LightActionCmd)); //send ack

				   #else
				   
                       ACT_PRINT("light debug: send to mac: %02x %02x %02x %02x %02x %02x\r\n",
    				   	mac_bkp[0],mac_bkp[1],mac_bkp[2],mac_bkp[3],mac_bkp[4],mac_bkp[5]);
                       esp_now_send(mac_bkp, (uint8*)(&light_cmd), sizeof(LightActionCmd)); //send ack
				   #endif
		}else{
			ACT_PRINT("SOURCE MAC CHEK FAIL \r\n");
   		}
	}else{
		ACT_PRINT("cmd check sum error\r\n");
	}

}

void ICACHE_FLASH_ATTR light_action_init()
{
	ACT_PRINT("===============\r\n");
    ACT_PRINT("CHANNEL : %d \r\n",wifi_get_channel());
	ACT_PRINT("===============\r\n");
	
	if (esp_now_init()==0) {
		os_printf("direct link  init ok\n");
		esp_now_register_recv_cb(light_action_rcv_cb);
	} else {
		os_printf("dl init failed\n");
	}

	    esp_now_set_self_role(2);  //role 1: switch   ;  role 2 : light;
}

void ICACHE_FLASH_ATTR light_action_deinit()
{

    esp_now_unregister_recv_cb(); 
    esp_now_deinit();
}

#elif LIGHT_SWITCH

#define ACTION_RETRY_NUM  3
#define ACTION_RETRY_TIMER_MS  200
typedef void (*ActionToutCallback)(uint32* act_idx);


typedef struct{
    uint32 sequence;
    ActionStatus status;
    os_timer_t req_timer;
    uint32 retry_num;
    uint32 retry_expire;
    ActionToutCallback actionToutCb;
    LightActionCmd lightActionCmd;
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
	check_light_cmd_result()
{
    int i;
	for(i=0;i<channel_num;i++){
        if(actionReqStatus[channel_group[i]].status == ACT_REQ) return false;
	}
	return true;
}


void ICACHE_FLASH_ATTR
	action_ack_callback()
{
	if( check_light_cmd_result() ){
		if(channel_cur == 14){
		    ACT_PRINT("release power\r\n");
		    _SWITCH_GPIO_RELEASE();
		}else{
			ACT_PRINT("SEND NEXT CHANNEL: %d \r\n",++channel_cur);
			switch_send_channel_cmd(channel_cur,pwm_chn_num, pwm_duty, pwm_period);
		}

	}

}

bool ICACHE_FLASH_ATTR
	check_light_sync_result()
{
    int i;
	for(i=0;i<CMD_NUM;i++){ //CMD NUM QUEALS PRACTICAL LIGHT NUMBER
        if(actionReqStatus[i].status == ACT_REQ ) return false;
	}
	return true;
}


void ICACHE_FLASH_ATTR
	action_sync_callback()
{
	if( check_light_sync_result() ){
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
			//switch_send_channel_cmd(channel_cur,pwm_chn_num, pwm_duty, pwm_period);
			switch_set_sync_param(channel_cur);
		}
		#endif

	}

}




 void ICACHE_FLASH_ATTR
 	switch_set_light_retry(void* arg)
{
    uint32 _idx = *((uint32*)arg);
    LightActionCmd* action_retry_cmd = &(actionReqStatus[_idx].lightActionCmd);
    Action_SendStatus* action_status = &(actionReqStatus[_idx]);
    
    if((action_retry_cmd->sequence == action_status->sequence)){
        if(action_status->status== ACT_REQ){
            esp_now_send((uint8*)LIGHT_MAC[_idx], (uint8*)action_retry_cmd, sizeof(LightActionCmd));
            action_status->retry_num++;
			if(action_status->retry_num < ACTION_RETRY_NUM){
                os_timer_arm( &action_status->req_timer, action_status->retry_expire,0);
			}else{
                ACT_PRINT("retry num exceed..stop retry, cmd type: %d\r\n",action_retry_cmd->type);
				action_status->status = ACT_TIME_OUT;
				
				if(action_retry_cmd->type==ACT_TYPE_SYNC){
					action_sync_callback();
				}else if(action_retry_cmd->type==ACT_DATA){
				    action_ack_callback();
				}
			}
    	}else{
            ACT_PRINT("STATUS error : %d\r\n",action_status->status);
    	}
        
    }else if(action_retry_cmd->sequence   <   action_status->sequence){
        ACT_PRINT("action updated...,cancel retry ...\r\n");
    }


}

void ICACHE_FLASH_ATTR switch_send_channel_cmd(uint8 chn,uint32 channelNum, uint32* duty, uint32 period)
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
			switch_set_light_param(channel_group[i], channelNum, duty, period);
			
		}
	}else{
		action_ack_callback();//next channel;
    }
}


extern uint32 user_get_battery_voltage_mv();

void ICACHE_FLASH_ATTR switch_set_light_param(uint8 idx, uint32 channelNum, uint32* duty, uint32 period)
{
    os_timer_disarm(&actionReqStatus[idx].req_timer); //disarm retry timer;
    actionReqStatus[idx].sequence+=1 ;//send another seq of cmd
    actionReqStatus[idx].status= ACT_REQ;
    actionReqStatus[idx].lightActionCmd.cmd_index = idx;
    actionReqStatus[idx].retry_num = 0;
    
    LightActionCmd light_cmd;
    light_cmd.cmd_index = idx;
    light_cmd.sequence = actionReqStatus[idx].sequence; //send another seq of cmd
    light_cmd.type = ACT_DATA;
    light_cmd.pwm_num = channelNum;
	light_cmd.wifiChannel = wifi_get_channel();
	light_cmd.battery_voltage_mv=user_get_battery_voltage_mv();
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
    light_action_set_csum(&light_cmd);

    #if ACT_DEBUG
	ACT_PRINT("send to :\r\n");
	ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
		                                               LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
	int j;
    for(j=0;j<sizeof(LightActionCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
	ACT_PRINT("\r\n");
    #endif
    esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&light_cmd, sizeof(LightActionCmd));

    os_memcpy(  &(actionReqStatus[idx].lightActionCmd), &light_cmd, sizeof(LightActionCmd));
    os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
	
}

void ICACHE_FLASH_ATTR 
	switch_set_sync_param(uint8 channel)
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
        
            LightActionCmd light_cmd;
            light_cmd.cmd_index = idx;
            light_cmd.sequence = actionReqStatus[idx].sequence; //send another seq of cmd
            light_cmd.type = ACT_TYPE_SYNC;
        	light_cmd.wifiChannel= wifi_get_channel();
			ACT_PRINT("CMD SEND CHANNLE: %d \r\n",light_cmd.wifiChannel);
       	
        	uint8 mac_buf[6] = {0};
        	wifi_get_macaddr(STATION_IF,mac_buf);
        	os_printf("source mac: %02x %02x %02x %02x %02x %02x\r\n",mac_buf[0],mac_buf[1],mac_buf[2],mac_buf[3],mac_buf[4],mac_buf[5]);
            os_memcpy(light_cmd.source_mac,mac_buf,sizeof(mac_buf));
            light_action_set_csum(&light_cmd);
        
            #if ACT_DEBUG
        	ACT_PRINT("send to :\r\n");
        	ACT_PRINT("MAC: %02X %02X %02X %02X %02X %02X\r\n",LIGHT_MAC[idx][0],LIGHT_MAC[idx][1],LIGHT_MAC[idx][2],
        		                                               LIGHT_MAC[idx][3],LIGHT_MAC[idx][4],LIGHT_MAC[idx][5]);
        	int j;
            for(j=0;j<sizeof(LightActionCmd);j++) ACT_PRINT("%02x ",*((uint8*)(&light_cmd)+j));
        	ACT_PRINT("\r\n");
            #endif
            esp_now_send((uint8*)LIGHT_MAC[idx], (uint8*)&light_cmd, sizeof(LightActionCmd));
            os_memcpy(  &(actionReqStatus[idx].lightActionCmd), &light_cmd, sizeof(LightActionCmd));
            os_timer_arm( &actionReqStatus[idx].req_timer, actionReqStatus[idx].retry_expire,0);
    	}
	}
	if(skip_flg){
		action_sync_callback();
	}
}




void ICACHE_FLASH_ATTR
	switch_channel_sync_start()
{
	int i;
	for(i=0;i<LIGHT_DEV_NUM;i++){
		actionReqStatus[i].wifichannel = 0;
	}
	channel_cur = 1;
	switch_set_sync_param(channel_cur);
}

void ICACHE_FLASH_ATTR switch_action_rcv_cb(u8 *macaddr, u8 *data, u8 len)
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

    LightActionCmd light_data ;
    os_memcpy( (uint8*)(&light_data),data, len);
    uint32 light_index=light_data.cmd_index;


	if(light_action_cmd_validate(&light_data) ){
		ACT_PRINT("cmd check sum OK\r\n");

       if(0 == os_memcmp(light_data.source_mac+1, LIGHT_MAC[light_index]+1,sizeof(SWITCH_MAC)-1)){
               ACT_PRINT("switch MAC match...\r\n");
       	uint32 _idx = light_data.cmd_index;
       	if(light_data.sequence == actionReqStatus[_idx].sequence && light_data.type == ACT_TYPE_ACK ){
       		actionReqStatus[_idx].status = ACT_ACK;
       		ACT_PRINT("cmd %d ack \r\n",_idx);
			ACT_PRINT("cmd channel : %d \r\n",light_data.wifiChannel);
			ACT_PRINT("SELF CHANNEL: %d \r\n",wifi_get_channel());
			action_ack_callback();
       	}else if(light_data.sequence == actionReqStatus[_idx].sequence && light_data.type == ACT_TYPE_SYNC){
			actionReqStatus[_idx].status = ACT_ACK;
			if(wifi_get_channel()==light_data.wifiChannel){
			    actionReqStatus[_idx].wifichannel = light_data.wifiChannel;
       		    ACT_PRINT("cmd %d sync,@ CHANNEL %d \r\n",_idx,actionReqStatus[_idx].wifichannel);
			    action_sync_callback();
			}else{
				ACT_PRINT("MESH SYNC CHANNEL ERROR, get channel : %d , data_channel : %d\r\n",wifi_get_channel,light_data.wifiChannel);
			}
		}else{
            if(light_data.sequence != actionReqStatus[_idx].sequence) ACT_PRINT("seq error\r\n");
       	    if(light_data.type != ACT_TYPE_ACK ) ACT_PRINT("already ack \r\n");
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

void ICACHE_FLASH_ATTR switch_ActionInit()
{
    uint8 i;
	int e_res;

	
	if (esp_now_init()==0) {
		os_printf("direct link  init ok\n");
		esp_now_register_recv_cb(switch_action_rcv_cb);
	} else {
		os_printf("dl init failed\n");
	}
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);  //role 1: switch   ;  role 2 : light;
	//esp_now_set_kok(esp_now_key, ESPNOW_KEY_LEN);


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
        actionReqStatus[i].actionToutCb = (ActionToutCallback)switch_set_light_retry;
        
        os_memset(&(actionReqStatus[i].lightActionCmd),0,sizeof(LightActionCmd));
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

void ICACHE_FLASH_ATTR switch_ActionDeinit()
{
    esp_now_unregister_recv_cb(); 
    esp_now_deinit();
}

#endif













