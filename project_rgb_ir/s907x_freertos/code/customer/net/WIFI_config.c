#include <stdio.h>
#include <string.h>

#include "net/wlan/wlan.h"
#include "net/wlan/wlan_defs.h"
#include "common/framework/net_ctrl.h"
#include "common/framework/sys_ctrl/sys_ctrl.h"

#include "common/framework/sysinfo.h"

#include "kernel/FreeRTOS/event_groups.h"

#include "sys/ZG_system.h"
#include "driver/chip/hal_wdg.h"


static uint8_t mac_addr[6];
static char mac_addr_str[13];

char ip_info[20] = "10.10.123.3";

static wifi_info_t wifi_set;

static ZG_wifi_mode_t wifi_mode;

static uint8_t WF_SSID[33],WF_PWD[65];

static EventGroupHandle_t wifi_event_group;

static TimerHandle_t rstimer;
static uint8_t reset_count,sec;

void wifi_scan_ap()
{
  
  wlan_sta_scan_once();

}

int ZG_restart_system(void)
{
	HAL_Status status = HAL_ERROR;
	WDG_HwInitParam hwParam;
	WDG_InitParam param;

	hwParam.event = WDG_EVT_RESET_CPU;
	hwParam.resetCpuMode = WDG_RESET_CPU_CORE;
	hwParam.timeout = WDG_TIMEOUT_2SEC;
	hwParam.resetCycle = 1;

	param.hw = hwParam;

	status = HAL_WDG_Init(&param);
	if (status != HAL_OK)
		printf("Wdg Init Error %d\n", status);
	else
		printf("WatchDog ResetCpu Mode Started.\n");
    HAL_WDG_Start();
	return status;
}

void rstimer_cb(void *arg)
{
 sec++;

/*取消计数恢复出厂功能*/
 if(sec >= 8){

    printf("\ndisarm power on restore function\n");
	sec = 0; 
	xTimerStop(rstimer,0); 
	reset_count = 0;
	ZG_data_save(RESTORE_STORE,&reset_count);
 }
}

int Restore_factory_settings_func(reset_type_t type)
{
	int return_type = 0;
	ZG_data_read(RESTORE_STORE,&reset_count);
	if(reset_count == 0xff){

	  reset_count = 0;
	}
	printf("\nreset count : %d \n",reset_count);
	if(type == RESET_BY_POWER){

	  reset_count++;
	}else{

      reset_count = 3;
	  //restart system 
      ZG_restart_system();
	}

	if(reset_count >= 4){

		reset_count = 0;
        return_type = -1;
	}

	ZG_data_save(RESTORE_STORE,&reset_count);
	ZG_store_fast();

	if(rstimer == NULL)
	   rstimer = xTimerCreate("timer_hb",(1000 / portTICK_RATE_MS),pdTRUE,(void *)0,rstimer_cb);
	xTimerStart(rstimer,0);

	return return_type;
}


static void save_ssid(uint8_t *ptr,int len)
{
	if(memcmp(WF_SSID,ptr,len) != 0 ){

		printf("store: ssid -> %s\n",ptr);
		uint8_t ssid_t[33] = {0};

		memset(WF_SSID,0,33);
		memcpy(WF_SSID,ptr,len);
		ssid_t[0] = len;
		memcpy(ssid_t + 1,ptr,len);
		ZG_data_save(DEV_SSID_STORE,ssid_t);
	}
}

static void save_password(uint8_t *ptr,int len)
{
	if( memcmp(WF_PWD,ptr,len) != 0){

		printf("store: password -> %s\n",ptr);
		uint8_t pwd_t[65] = {0};

		memset(WF_PWD,0,65);
		memcpy(WF_PWD,ptr,len);
		pwd_t[0] = len;
		memcpy(pwd_t + 1,ptr,len);
	    ZG_data_save(DEV_PWD_STORE,pwd_t);
	}
}

static void save_wifi_mode(ZG_wifi_mode_t mode)
{
	if(wifi_mode != mode ){
	  wifi_mode = mode;
      printf("wifi mode %d -> %d\n",mode,wifi_mode );
 	  ZG_data_save(WIFI_MODE_STORE,&wifi_mode);
	}
}

void net_event_callback(uint32_t event, uint32_t data,void *arg)
{
	uint16_t type = EVENT_SUBTYPE(event);

	printf("%s msg (%u, %u)\n", __func__, type, data);
	switch (type) {
	case NET_CTRL_MSG_WLAN_CONNECTED:
		printf("NET_CTRL_MSG_WLAN_CONNECTED\n");
		break;
	case NET_CTRL_MSG_WLAN_SCAN_SUCCESS:
		printf("NET_CTRL_MSG_WLAN_SCAN_SUCCESS\n");
	      wlan_sta_scan_results_t results; 
		  results.size = 10;    //最多获取 10 个 AP 节点信息 
		  results.ap = malloc(results.size * sizeof(wlan_sta_ap_t)); 
		  
		  if (results.ap == NULL) { 

		     printf("wifi:scan malloc fail\n");
		  } 
		  if (wlan_sta_scan_result(&results) == 0) { 

				char *scan_temp = malloc(100);
				char *scan_buff = malloc(1000);
				memset((uint8_t *)scan_buff,0,1000);
			            /**
		         +ok=
		        Ch,SSID,BSSID,Security,Indicator
		        11,hings-net,14:75:90:79:20:87,WPAPSKWPA2PSK/AES,27
		        7,Xiaomi_95A1,78:11:DC:1C:95:A2,WPAPSKWPA2PSK/AES,32
		         */
			    strcat(scan_buff,"+ok=\n");
		        strcat(scan_buff,"Ch,SSID,BSSID,Security,Indicator\n");
		        for (int i = 0; i < results.num; i++) {

		            memset(scan_temp,0,100);
		            sprintf(scan_temp,
		                "%d,%s,%02X:%02X:%02X:%02X:%02X:%02X,%s,%d\n",
		                results.ap[i].channel,
		                (char *)results.ap[i].ssid.ssid,
		                results.ap[i].bssid[0], results.ap[i].bssid[1], results.ap[i].bssid[2], results.ap[i].bssid[3], results.ap[i].bssid[4], results.ap[i].bssid[5],
		                "WPAPSKWPA2PSK/AES",
		                129 + results.ap[i].rssi);
		                   
		            strcat(scan_buff,scan_temp);
		        }
		        UDP_Send(scan_buff,strlen(scan_buff));
		  }else{

		     printf("scan fail\n");
		  } 
		  free(results.ap);
		break;
	case NET_CTRL_MSG_WLAN_CONNECT_FAILED:
		printf("NET_CTRL_MSG_WLAN_CONNECT_FAILED\n");
	    xEventGroupClearBits(wifi_event_group, BIT0);
		break;
	case NET_CTRL_MSG_NETWORK_UP:

		printf("NET_CTRL_MSG_NETWORK_UP: %s\n", ip_info);
		if(memcmp(ip_info,"10.10.123.3",11) != 0){

			xEventGroupSetBits(wifi_event_group, BIT0);
		    ZG_event_send(CONNECTED_ROUTER_EVENT);
        }
		break;
	case NET_CTRL_MSG_NETWORK_DOWN:
		printf("NET_CTRL_MSG_NETWORK_DOWN\n");
	
		break;
	case NET_CTRL_MSG_WLAN_DISCONNECTED:
	    printf("NET_CTRL_MSG_WLAN_DISCONNECTED\n");
	    xEventGroupClearBits(wifi_event_group, BIT0);
	case NET_CTRL_MSG_WLAN_SCAN_FAILED:
	case NET_CTRL_MSG_WLAN_4WAY_HANDSHAKE_FAILED:
		printf("do nothing msg (%u, %u)\n", type, data);
		break;
	default:
		printf("unknown msg (%u, %u)\n", type, data);
		break;
	}
}

int get_wifi_connection_status(int xTicksToWait)
{
  return xEventGroupWaitBits(wifi_event_group, BIT0, false, true, xTicksToWait);
}


void wifi_ssid_conf(uint8_t * str,int len)
{
   memset(wifi_set.ssid,0,33);
   memcpy(wifi_set.ssid,str,len);
   wifi_set.ssid_len = len;
   save_ssid(wifi_set.ssid,len);
}

void wifi_password_conf(uint8_t *str,int len)
{
  memset(wifi_set.pwd,0,65);
  if(len != 0){

	  memcpy(wifi_set.pwd,str,len);
	  wifi_set.pwd_len = len;
	  save_password(wifi_set.pwd,len);
  }else{
    
  	  save_password(wifi_set.pwd,64);
  }
 
}


static void wifi_STA_Start()
{
  net_switch_mode(WLAN_MODE_STA);
  wlan_sta_set(wifi_set.ssid, wifi_set.ssid_len, wifi_set.pwd);
  wlan_sta_enable();
  save_wifi_mode(ZG_STA_MODE);
  printf("wifi_init_sta finished.SSID:%s password:%s\n",
	wifi_set.ssid, wifi_set.pwd);
}


static void AP_IP_Config()
{
  // net_config(struct netif *nif, uint8_t bring_up);
}

static void wifi_AP_Start()
{
	net_switch_mode(WLAN_MODE_HOSTAP);
	/* disable AP to set params*/
	wlan_ap_disable();

	wlan_ap_set(wifi_set.ssid, wifi_set.ssid_len, wifi_set.pwd);

	wlan_ap_enable();
   
	printf("wifi_init_softap finished.SSID:%s password:%s\n",
	wifi_set.ssid, wifi_set.pwd);

	save_wifi_mode(ZG_AP_MODE);
}

void wifi_Adapter_start(ZG_wifi_mode_t mode)
{
  if(mode == ZG_AP_MODE){

  	wifi_AP_Start();
  }else{

  	wifi_STA_Start();
  }
}


void factory_ap_conf(void)
{
	printf("factory ap mode \n");
	char dev_name[20] = {0};
	sprintf(dev_name,"LEDnet%02X%02X%02X",mac_addr[3],mac_addr[4],mac_addr[5]);
	printf("device name:%s\n",dev_name);
	/* switch to ap mode */
	net_switch_mode(WLAN_MODE_HOSTAP);
    wifi_ssid_conf((uint8_t *)dev_name,12);
    wifi_password_conf(NULL,64);
    wifi_Adapter_start(ZG_AP_MODE);
    ZG_store_fast();
	tcp_client_deinit();
}

int net_service_init(void)
{
	observer_base *observer;

	printf("foneric_net_service_init, register network observer\n");
	observer = sys_callback_observer_create(CTRL_MSG_TYPE_NETWORK,
						NET_CTRL_MSG_ALL,
						net_event_callback,NULL);
	if (observer == NULL)
		return -1;
	if (sys_ctrl_attach(observer) != 0)
		return -1;

	return 0;
}

void wifi_get_ip_info(char *ip_addr)
{
  memcpy(ip_addr,ip_info,strlen(ip_info));
}

void wifi_get_mac_info(char *mac_str)
{
  memcpy(mac_str,mac_addr_str,12);
}

int get_ap_rssi()
{
  wlan_sta_ap_t *ap = malloc(sizeof(wlan_sta_ap_t)); 
  if (ap == NULL) { 
     printf("get ap info error\n");
  } 
  wlan_sta_ap_info(ap); 
  return ap->rssi;
}

void wifi_get_conf_msg(wifi_info_t *wifi_msg)
{
  *wifi_msg = wifi_set;
}

ZG_wifi_mode_t wifi_get_mode()
{
	return wifi_mode;
}

void WIFI_Init()
{
	wifi_event_group = xEventGroupCreate();
	wlan_get_mac_addr(g_wlan_netif, mac_addr, 6);
    sprintf(mac_addr_str,"%02X%02X%02X%02X%02X%02X",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
    printf("mac sting:%s\n", mac_addr_str);
    net_service_init();

    if(Restore_factory_settings_func(RESET_BY_POWER) == -1){

    	goto factory_ap_mode;
    }

    ZG_data_read(WIFI_MODE_STORE,&wifi_mode);
    if(wifi_mode > 2 || wifi_mode == 0){

      goto factory_ap_mode;
    }else{
      
      uint8_t tmp[65] = {0};
      ZG_data_read(DEV_SSID_STORE,tmp);
      printf("system read:ssid length : %d\n", tmp[0]);
      if(tmp[0] > 32){ // length

         goto factory_ap_mode;
      }else{
        wifi_set.ssid_len = tmp[0];
      	memcpy(wifi_set.ssid,tmp + 1,tmp[0]);
      	printf("system read:ssid:%s\n", wifi_set.ssid);
      }
      ZG_data_read(DEV_PWD_STORE,tmp);
      printf("system read:password length : %d\n", tmp[0]);
      if(tmp[0] > 64){ // length

         goto factory_ap_mode;
      }else{

      	memcpy(wifi_set.pwd,tmp + 1,tmp[0]);
      	memcpy(WF_PWD,wifi_set.pwd,tmp[0]);
      }
      printf("system read:password : %s\n",wifi_set.pwd);
      wifi_Adapter_start(wifi_mode);
    }

    return;

    factory_ap_mode:
       factory_ap_conf();
       ZG_event_send(FACTORY_SETTING_EVENT);
} 