#ifndef _ZG_SYS_H_
#define _ZG_SYS_H_

/*
 task priority
 tcp sever 6 
 udp 5
 tcp client 7
 store  2
 system 1

*/

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "common/framework/platform_init.h"
#include "kernel/os/os_time.h"

#include "net/WIFI_config.h"

#include "socket/udp_perf.h"
#include "socket/tcp_perf.h"
#include "socket/tcp_client.h"

#include "protocol_handle/protocol_handle.h"
#include "protocol_handle/proto_parser_pack.h"

#include "sys/encryption.h"
#include "sys/store.h"
#include "sys/RTC_time.h"
#include "sys/fota.h"

#include "light/clock.h"
#include "light/auto_report.h"
#include "light/Light_fun.h"

typedef int32_t esp_err_t;

/* Definitions for error constants. */
#define ESP_OK          0       /*!< esp_err_t value indicating success (no error) */
#define ESP_FAIL        -1      /*!< Generic esp_err_t code indicating failure */

#define BIT0    0x00000001

#define ZG_system_time()  OS_GetTicks()
/*

#include "fota.h"
	*/
void ZG_system_start();

esp_err_t ZG_device_info_conf(char *dev_id,char * ver,uint8_t dev_code,uint8_t pro_ver);

void ZG_device_mcu_ver_conf(char * ver);

char *ZG_get_device_version();
char *ZG_get_device_id();
char *ZG_get_mcu_version();
uint8_t ZG_get_device_code();
uint8_t ZG_get_device_protocol_version();

typedef enum {
    FACTORY_SETTING_EVENT = 0,
    CONNECTED_ROUTER_EVENT,
    OTA_EVENT
} ZG_system_event_t;


/**
  * @brief  Application specified event callback function
  *
  * @param  void *ctx : reserved for user
  * @param  system_event_t *event : event type defined in this file
  *
  * @return ESP_OK : succeed
  * @return others : fail
  */
typedef esp_err_t (*ZG_system_event_cb_t)(void *ctx, ZG_system_event_t *event);

/**
  * @brief  Initialize event loop
  *         Create the event handler and task
  *
  * @param  system_event_cb_t cb : application specified event callback, it can be modified by call esp_event_set_cb
  * @param  void *ctx : reserved for user
  *
  * @return ESP_OK : succeed
  * @return others : fail
  */
esp_err_t ZG_event_loop_init(ZG_system_event_cb_t cb, void *ctx);
esp_err_t ZG_event_send(ZG_system_event_t event);

#ifdef __cplusplus
}
#endif



#endif 