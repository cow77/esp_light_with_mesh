#include "user_light_hint.h"
#include "osapi.h"
#include "os_type.h"
#include "user_light.h"
#include "user_light_adj.h"

#if LIGHT_DEVICE

os_timer_t light_hint_t;
#define LIGHT_INFO os_printf


LOCAL void ICACHE_FLASH_ATTR
    light_blink(uint32 color)
{
	static bool blink_flg = true;
	if(blink_flg){
        switch(color){
			case HINT_WHITE:
				user_light_set_duty(0,LIGHT_RED);
				user_light_set_duty(0,LIGHT_GREEN);
				user_light_set_duty(0,LIGHT_BLUE);
				user_light_set_duty(5000,LIGHT_COLD_WHITE);
				user_light_set_duty(5000,LIGHT_WARM_WHITE);
				break;
			case HINT_RED:
				user_light_set_duty(10000,LIGHT_RED);
				user_light_set_duty(0,LIGHT_GREEN);
				user_light_set_duty(0,LIGHT_BLUE);
				user_light_set_duty(2000,LIGHT_COLD_WHITE);
				user_light_set_duty(2000,LIGHT_WARM_WHITE);
				break;
			case HINT_GREEN:
				user_light_set_duty(0,LIGHT_RED);
				user_light_set_duty(10000,LIGHT_GREEN);
				user_light_set_duty(0,LIGHT_BLUE);
				user_light_set_duty(2000,LIGHT_COLD_WHITE);
				user_light_set_duty(2000,LIGHT_WARM_WHITE);
				break;
			case HINT_BLUE:
				user_light_set_duty(0,LIGHT_RED);
				user_light_set_duty(0,LIGHT_GREEN);
				user_light_set_duty(10000,LIGHT_BLUE);
				user_light_set_duty(2000,LIGHT_COLD_WHITE);
				user_light_set_duty(2000,LIGHT_WARM_WHITE);
				break;
			default :
				break;
        }
		blink_flg = false;
	}else{
		user_light_set_duty(0,LIGHT_RED);
		user_light_set_duty(0,LIGHT_GREEN);
		user_light_set_duty(0,LIGHT_BLUE);
		user_light_set_duty(0,LIGHT_COLD_WHITE);
		user_light_set_duty(0,LIGHT_WARM_WHITE);
		blink_flg = true;
	}
	pwm_start();
}

void ICACHE_FLASH_ATTR
	light_blinkStart(uint32 COLOR)
{
    os_timer_disarm(&light_hint_t);
    os_timer_setfn(&light_hint_t,light_blink,COLOR);
    os_timer_arm(&light_hint_t,1000,1);
}

LOCAL void ICACHE_FLASH_ATTR
	light_shade(uint32 color)
{
    static bool color_flg = true;
    if(color_flg){
        switch(color){
        case HINT_GREEN:
            light_set_aim(0,20000,0,2000,2000,1000,0);
            break;
        case HINT_RED:
            light_set_aim(20000,0,0,2000,2000,1000,0);
            break;
        case HINT_BLUE:
            light_set_aim(0,0,20000,2000,2000,1000,0);
            break;
        case HINT_WHITE:
            light_set_aim(0,0,0,20000,20000,1000,0);
            break;
        }        
        color_flg = false;
    }
    else{
        light_set_aim(0,0,0,1000,1000,1000,0);
        color_flg = true;
    }
}



void ICACHE_FLASH_ATTR
	light_shadeStart(uint32 color,uint32 t)
{
    LIGHT_INFO("LIGHT SHADE START");
    os_timer_disarm(&light_hint_t);
    os_timer_setfn(&light_hint_t,light_shade,color);
    os_timer_arm(&light_hint_t,t,1);
}


void ICACHE_FLASH_ATTR
	light_hint_stop(uint32 color)
{
    os_timer_disarm(&light_hint_t);
	switch(color){
        case HINT_RED:
			light_set_aim(2000,0,0,0,0,1000,0);
			break;
		case HINT_GREEN:
			light_set_aim(0,2000,0,0,0,1000,0);
			break;
		case HINT_BLUE:
			light_set_aim(0,0,2000,0,0,1000,0);
			break;
		case HINT_WHITE:
			light_set_aim(0,0,0,0,2000,1000,0);
			break;
		default:
			light_set_aim(0,0,0,0,2000,1000,0);
			break;
	}
}

#endif

