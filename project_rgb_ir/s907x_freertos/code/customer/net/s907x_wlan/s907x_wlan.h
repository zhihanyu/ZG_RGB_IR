#ifndef _S907X_WLAN_H_
#define _S907X_WLAN_H_

#include "s907x.h"
#include "lwip_conf.h"
#include "wifi_config.h"








void wifi_event_callback(void);
void wlan_sta_scan_once(void);
int s907x_wlan_start(u8 wf_m, wifi_info_t *wf_inf);
int wlan_get_mac_addr(u8 netif_id, u8* mac, int mac_len);













#endif



