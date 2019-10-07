#include <stdio.h>
#include <string.h>
#include "net/wlan/wlan.h"
#include "net/wlan/wlan_defs.h"
#include "common/framework/net_ctrl.h"
#include "common/framework/platform_init.h"
#include "lwip/inet.h"
#include "common/framework/sys_ctrl/sys_ctrl.h"


#include "kernel/FreeRTOS/FreeRTOS.h"
#include "kernel/FreeRTOS/task.h"
#include "kernel/FreeRTOS/event_groups.h"
#include "kernel/FreeRTOS/queue.h"

#include "ZG_system.h"

static char MCU_ver[30];
static char Device_ver[30];
static char Device_ID[30];

static uint8_t light_protocol_ver;

static uint8_t device_code;

static char s_event_init_flag = 0;
static void *s_event_queue = NULL;
static ZG_system_event_cb_t s_event_handler_cb = NULL;
static void *s_event_ctx = NULL;

esp_err_t ZG_device_info_conf(char *dev_id,char * ver,uint8_t dev_code,uint8_t pro_ver)
{
  if(dev_code == 0 || dev_id == NULL || ver == NULL){
  	return ESP_FAIL;
  }
  strncpy(Device_ID,dev_id,strlen(dev_id));
  strncpy(Device_ver,ver,strlen(ver));
  device_code = dev_code;
  light_protocol_ver = pro_ver;
  printf("\n");
  printf("device id -> %s\n",Device_ID);
  printf("device version -> %s\n",Device_ver);
  printf("device code -> %02X \n",device_code);
  printf("device protocol version -> %d\n",light_protocol_ver);
  printf("\n");
  return ESP_OK;
  
}
uint8_t ZG_get_device_protocol_version()
{
  return light_protocol_ver;
}

void ZG_device_mcu_ver_conf(char * ver)
{
  strncpy(MCU_ver,ver,strlen(ver));
}

char *ZG_get_device_version()
{
  return Device_ver;
}

char *ZG_get_device_id()
{
	return Device_ID;
}

char *ZG_get_mcu_version()
{
	return MCU_ver;
}

uint8_t ZG_get_device_code()
{
	return device_code;
}

void ZG_system_start()
{
  store_init();
  light_init();
	WIFI_Init();

	tcp_server_task_init();
	udp_task_init();
	AES_Data_Func_init();
	tcp_Client_init();
}

static esp_err_t ZG_event_post_to_user(ZG_system_event_t *event)
{
    if (s_event_handler_cb) {
        return (*s_event_handler_cb)(s_event_ctx, event);
    }
    return ESP_OK;
}

static void ZG_event_loop_task(void *pvParameters)
{
    while (1) {
        ZG_system_event_t evt;
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) == pdPASS) {

            int ret = ZG_event_post_to_user(&evt);
            if (ret != ESP_OK) {
                printf( "post event to user fail!");
            }
        }
    }
}


esp_err_t ZG_event_send(ZG_system_event_t event)
{
    if (s_event_queue == NULL) {
        printf( "Event loop not initialized via esp_event_loop_init, but esp_event_send called");
        return  ESP_FAIL;
    }

    int ret = xQueueGenericSend(s_event_queue, &event, 0, queueSEND_TO_BACK);
    if (ret != pdPASS) {

        return ESP_FAIL;
    }
    return ESP_OK;
}

QueueHandle_t ZG_event_loop_get_queue(void)
{
    return s_event_queue;
}

esp_err_t ZG_event_loop_init(ZG_system_event_cb_t cb, void *ctx)
{
    if (s_event_init_flag) {
        return ESP_FAIL;
    }
    s_event_handler_cb = cb;
    s_event_ctx = ctx;
    s_event_queue = xQueueCreate(32, sizeof(ZG_system_event_t));
    if(s_event_queue == NULL)
        return  ESP_FAIL;;
    xTaskCreate(ZG_event_loop_task, "ZG_event_loop_task", 1024, NULL, configMAX_PRIORITIES - 6, NULL);

    s_event_handler_cb = cb;
    s_event_ctx = ctx;
    s_event_init_flag = 1;
    return ESP_OK;
}