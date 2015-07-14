#include "esp_send.h"
#include "ip_addr.h"
#include "espconn.h"
#include "ringbuf.h"
#include "osapi.h"
#include "mem.h"


#define SEND_DBG  os_printf

RINGBUF ringBuf;
#define ESP_SEND_QUEUE_LEN (1024*2)
#define ESP_SEND_BUF_LEN (1460)
uint8 espSendBuf[ESP_SEND_QUEUE_LEN];
os_event_t esp_SendProcTaskQueue[ESP_SEND_TASK_QUEUE_SIZE];
os_timer_t esp_send_t;


/******************************************************************************
 * FunctionName : espSendEnq
 * Description  : push the sending packet to a platform tx queue.
                  post an event to system task to send TCP data.
                  keep the data until the packet is sent out
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
    espSendEnq(uint8* data, uint16 length, struct espconn* pconn, EspPlatformMsgType MsgType ,EspSendTgt Target ,RINGBUF* r)
{
	SEND_DBG("++++++++++++\r\n%s\r\n+++++++++++++++\r\n",__func__);
	if( RINGBUF_Check(r,length+sizeof(EspSendFrame))){
		EspSendFrame sf;
		sf.dataLen = length;
		sf.dType = MsgType;
		sf.pConn = pconn;
		sf.dTgt = Target;

		uint8* pdata = (uint8*)&sf;
		RINGBUF_Push(r,pdata ,sizeof(EspSendFrame));
		
		pdata = data;
		RINGBUF_Push(r,pdata ,length);
		
		SEND_DBG("data enq: \r\n len: %d \r\n++++++++++++++++\r\n",length);
		SEND_DBG("pconn ori: 0x%08x\r\n",pconn);
		SEND_DBG("%s \r\n",data);
		SEND_DBG("+++++++++++++++++++\r\n");
		return 0;
	}else{
		return -1;
	}
}


/******************************************************************************
 * FunctionName : espSendDataRetry
 * Description  : if the result of espconn_send is not 0, that means failed to send data out, try sending again
*******************************************************************************/
void ICACHE_FLASH_ATTR
	espSendDataRetry(void* para)
{
	
	if(espSendQueueIsEmpty(espSendGetRingbuf())){
		return;
	}else{
		system_os_post(ESP_SEND_TASK_PRIO, 0, (os_param_t)espSendGetRingbuf());
	}
}


/******************************************************************************
 * FunctionName : espSendTask
 * Description  : handler for platform data send
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
	espSendTask(os_event_t *e)
{
	SEND_DBG("************\r\n%s\r\n*************\r\n",__func__);
	
	RINGBUF* r = (RINGBUF*)(e->par);
	
	os_timer_disarm(&esp_send_t);
	uint8 dataSend[ESP_SEND_BUF_LEN];
	os_memset(dataSend,0,sizeof(dataSend));
	EspSendFrame sf;

	uint8* pdata = (uint8*)&sf;

    RINGBUF_PullRaw(r,pdata,sizeof(EspSendFrame),0);

	SEND_DBG("get data length: %d \r\n",sf.dataLen);
	SEND_DBG("get pconn: 0x%08x \r\n",sf.pConn);
	SEND_DBG("get data type: %d \r\n",sf.dType);
	SEND_DBG("get data target: %d \r\n",sf.dTgt);

	if(sf.dataLen>ESP_SEND_BUF_LEN){
		SEND_DBG("WARNING: DATA TOO LONG ,DROP(<=%d)...\r\n",ESP_SEND_BUF_LEN);
		return -1;
	}

	if(sf.pConn==NULL){
		return -1;
	}else{
		
		RINGBUF_PullRaw(r,dataSend,sf.dataLen,sizeof(EspSendFrame));

    	sint8 res;
#ifdef CLIENT_SSL_ENABLE
		res = espconn_secure_sent(sf.pConn, dataSend, sf.dataLen);
#else
		res = espconn_sent(sf.pConn, dataSend, sf.dataLen);
#endif
    	SEND_DBG("pconn: 0x%08x \r\n",sf.pConn);
    	SEND_DBG("datalen: %d \r\n******************\r\n",sf.dataLen);
    	SEND_DBG("%s \r\n",dataSend);
    	SEND_DBG("*******************\r\n");

    	if(res==0){
    		SEND_DBG("espconn send ok, wait for callback...\r\n");
    		espSendQueueUpdate(espSendGetRingbuf());//		
    		if(sf.dTgt == TO_LOCAL){
    			espSendAck(espSendGetRingbuf());
    		}
    	}else{
    		SEND_DBG("espconn send fail, send again...\r\n");
    		os_timer_disarm(&esp_send_t);
    		os_timer_arm(&esp_send_t,50,0);
    	}
	}
	
}

/******************************************************************************
 * FunctionName : espSendGetRingbuf
 * Description  : return the pointer of the RINGBUF
*******************************************************************************/
RINGBUF* ICACHE_FLASH_ATTR espSendGetRingbuf()
{
	return &ringBuf;
}


/******************************************************************************
 * FunctionName : espSendQueueInit
 * Description  : Init for the RINGBUFFER and set a system task to process platform send task.
*******************************************************************************/
void ICACHE_FLASH_ATTR
	espSendQueueInit()
{
	os_memset(espSendBuf,0,sizeof(espSendBuf));
	SEND_DBG("sizeof(espSendBuf): %d\r\n",sizeof(espSendBuf));
	RINGBUF_Init(espSendGetRingbuf(),espSendBuf,sizeof(espSendBuf) );
	system_os_task(espSendTask, ESP_SEND_TASK_PRIO, esp_SendProcTaskQueue, ESP_SEND_TASK_QUEUE_SIZE);
	os_timer_disarm(&esp_send_t);
	os_timer_setfn(&esp_send_t,espSendDataRetry,espSendGetRingbuf());
}

/******************************************************************************
 * FunctionName : espSendQueueIsEmpty
 * Description  : check whether the BINGBUF is empty
*******************************************************************************/
bool ICACHE_FLASH_ATTR
	espSendQueueIsEmpty(RINGBUF* r)
{
	if(r->p_r == r->p_w) return true;
	else return false;
}

/******************************************************************************
 * FunctionName : espSendQueueUpdate
 * Description  : Free the 1st packet in tx queue
*******************************************************************************/
sint8 ICACHE_FLASH_ATTR
	espSendQueueUpdate(RINGBUF* r)
{
	EspSendFrame sf;
	
	RINGBUF_Pull(r,(uint8*)&sf,sizeof(EspSendFrame));
	SEND_DBG("get data length: %d \r\n",sf.dataLen);
	SEND_DBG("get pconn: 0x%08x \r\n",sf.pConn);
	SEND_DBG("get data type: %d \r\n",sf.dType);
	SEND_DBG("esp send queue drop : %d bytes \r\n",sf.dataLen);
	RINGBUF_Drop(r,sf.dataLen);
}

/******************************************************************************
 * FunctionName : espSendAck
 * Description  : In mesh network, there are some protocol to check whether it is capable to send the packets out.
                  After we have send the first packet in the sending queue, post an event to send other packets.
*******************************************************************************/
void ICACHE_FLASH_ATTR
	espSendAck(RINGBUF* r)
{
	SEND_DBG("test send finish ack\r\n");	
	if(espSendQueueIsEmpty(r))
		return;
	else 
		system_os_post(ESP_SEND_TASK_PRIO, 0, (os_param_t)r);
}


