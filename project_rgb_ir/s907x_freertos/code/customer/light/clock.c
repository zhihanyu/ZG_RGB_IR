
#include "sys/ZG_system.h"
#include "FreeRTOS.h"
#include "task.h"

static  uint8_t Is_timer_trigger = true;
static  uint8_t timing_buf[88];

static TaskHandle_t xHandleClock = NULL;
static void timercolok_thread(void *arg)
{
  struct rtc_time Real_time;
  for(;;){
 	uint8_t week_tem = 0;
 
 	get_RTC_time(&Real_time);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
     for(int i = 1; i < 7;i++) {
 
         /*hour min sec equal*/
    	 if(Real_time.tm_hour == timing_buf[i * 14 - 9] &&
    			 Real_time.tm_min == timing_buf[i * 14 - 8] &&
				 (Real_time.tm_sec - timing_buf[i * 14 - 7] < 3) &&
				 timing_buf[i * 14 - 13] == 0xF0){
            /*每周*/
 		    switch (Real_time.tm_wday) {
				case 1 :
				   week_tem = timing_buf[i * 14 - 6] & 0x02;
					   break;
				case 2 :
					week_tem = timing_buf[i * 14 - 6] & 0x04;
					  break;
				case 3 :
					week_tem = timing_buf[i * 14 - 6] & 0x08;
					  break;
				case 4 :
				    week_tem = timing_buf[i * 14 - 6] & 0x10;
					  break;
				case 5 :
					week_tem = timing_buf[i * 14 - 6] & 0x20;
					 break;
				case 6 :
					week_tem = timing_buf[i * 14 - 6] & 0x40;
					  break;
				case 7 :
					week_tem = timing_buf[i * 14 - 6] & 0x80;
					  break;
				default	:week_tem=0;
 		    }

			if(week_tem != 0 ||
			   (timing_buf[i * 14 - 12] == (Real_time.tm_year - 2000) &&
					   timing_buf[i * 14 - 11] == Real_time.tm_mon &&
					   timing_buf[i * 14 - 10] == Real_time.tm_mday &&
					   timing_buf[i * 14 - 6] == 0)){

				    if(Is_timer_trigger == true){

                        printf("clock:trigger\n");
				    	set_report_reson(TIMER_CONTROL);
						Is_timer_trigger = false;
						
                        /*开关值判断*/
						if(timing_buf[i * 14] == 0x0F ){
						   
                             set_led_switch(MODE_OFF);
						}else if(timing_buf[i * 14] == 0xF0 ){
  
							  set_led_switch(MODE_ON);
							
						}
						if(timing_buf[i * 14 - 5] == 0x61){/*静态颜色*/

							colort_struct_t color;
							memset(&color,0,sizeof(colort_struct_t));
							color.R = timing_buf[i * 14 - 4];
							color.G = timing_buf[i * 14 - 3];
							color.B = timing_buf[i * 14 - 2];
							set_led_mode(MODE_STATIC);
							Set_color(&color);
						 
						}else if(timing_buf[i * 14 - 5] >= 0x25 && timing_buf[i * 14 - 5] <= 0x38){/*内置模式*/

                           set_led_mode(timing_buf[i * 14 - 5]);
						   set_led_speed(timing_buf[i * 14 - 4]); 
						}
				    }
			}
 		}

		 if(Real_time.tm_sec - timing_buf[i * 14 - 7] < 3){

		 }else {
			 Is_timer_trigger = true;
		 }
     }
  }
  vTaskDelete(NULL);
}

void  timer_clock_init()
{
  
}

void timer_clock_Conf(uint8_t *dat,int len)
{
  memcpy(timing_buf,dat,len);
}

uint8_t * check_timer_clock_data()
{
	return timing_buf;
}

void timer_clock_start()
{
  if(xHandleClock == NULL){

      xTaskCreate(&timercolok_thread, "timercolok_thread", 1024, NULL,tskIDLE_PRIORITY + 1, &xHandleClock);
	  printf("clock : creat clock thread\n");
  }
}