#include "s907x_wlan.h"
#include "s907x_zg_config.h"
#include "lwip/ip_addr.h"
#include "dhcps.h"

#if ZG_BUILD

static sema_t scan_sema = NULL;
static void s907x_wlan_scan_cb(s907x_scan_result_t *presult)
{
    wlan_target_infor_t *target;
    ASSERT(presult);
    target = (wlan_target_infor_t*)presult->context; 
    if(presult->max_nums < 1) {
        USER_DBG("scan result : no network!\n");	
        if(target) {
            wl_send_sema((sema_t*)&target->sema);
        } else if(scan_sema){
            wl_send_sema(&scan_sema);
        } 
        return;
    } 
    if(presult->id == 0){
        USER_DBG("-----------------------scan result------------------\n");
    } 
    USER_DBG("scan id %02d max ap %02d bssid "MAC_FMT" channel %02d security = %d rssi %02d ssid %s \n", presult->id, presult->max_nums, presult->scan_info.bssid[0], 
                  presult->scan_info.bssid[1],presult->scan_info.bssid[2],presult->scan_info.bssid[3],presult->scan_info.bssid[4],presult->scan_info.bssid[5], 
                  presult->scan_info.channel, presult->scan_info.security,  presult->scan_info.rssi, presult->scan_info.ssid);
    if(target && presult->scan_info.ssid_len && !strncmp(target->ssid, presult->scan_info.ssid, presult->scan_info.ssid_len)) {
	target->match =  TRUE;
        target->channel = presult->scan_info.channel;
        target->security = presult->scan_info.security;
    }
    if(presult->id == presult->max_nums - 1) {
            if(target) {
                    wl_send_sema((sema_t*)&target->sema);
            } else if(scan_sema) {
                    wl_send_sema(&scan_sema);
            }
    }
}

void wlan_sta_scan_once(void)
{
    s907x_wlan_scan(s907x_wlan_scan_cb, S907X_DEFAULT_SCAN_AP_NUMS, NULL);
}

static int wlan_connect_normal(s907x_sta_init_t *init)
{
    int ret = HAL_OK;
    wlan_target_infor_t target;
    
    target.match = FALSE;
    strcpy(target.ssid, init->ssid);
    USER_DBG("target.ssid:%s\n",target.ssid);
    wl_init_sema(&target.sema, 0, sema_binary);
    ret = s907x_wlan_scan(s907x_wlan_scan_cb, S907X_DEFAULT_SCAN_AP_NUMS, &target);
    USER_DBG("ret = %d\n",ret);
    if(ret) {
        wl_free_sema(&target.sema);
        return ret;
    }
    USER_DBG("ret = %d\n",ret);
    wl_wait_sema(&target.sema, portMAX_DELAY);		
    wl_free_sema(&target.sema);
    if(target.match){
        Z_DEBUG();
        //set security
        init->security = target.security;
        USER_DBG("target.security = %d\n", target.security);
        //set connection mode
        init->conn.mode	=  CONN_MODE_BLOCKING;
        init->conn.blocking_timeout = ZG_WIFI_BLOCK_TIME;
        USER_DBG("ret = %d\n",ret);
        ret = s907x_wlan_start_sta(init);
        USER_DBG("ret = %d\n",ret);

        //info for test
        USER_DBG("init->ssid:%s\n", init->ssid);
        USER_DBG("init->ssid_len:%d\n", init->ssid_len);
        USER_DBG("init->password:%s\n", init->password);
        USER_DBG("init->password_len:%d\n", init->password_len);
        USER_DBG("init->security:%s\n", init->security);
    }else{
        USER_DBG("ret = %d\n",ret);
        ret = HAL_ERROR;
    }
    USER_DBG("ret = %d\n",ret);
    return ret;
}

static int wlan_set_sta(wifi_info_t *wf_info)
{
    int ret = HAL_OK;
    u8 mac[6];
    u8 host_name[32] = {0};
    s907x_sta_init_t s907x_sta_init = {0};
      
    s907x_wlan_get_mac_address(S907X_DEV0_ID,mac);
    sprintf(host_name, "sta_%02X%02X%02X",mac[3],mac[4],mac[5]);
    lwip_set_hostname(LwIP_GetNetif(S907X_DEV0_ID),host_name);
    
    s907x_sta_init.ssid = wf_info->ssid;
    s907x_sta_init.ssid_len = wf_info->ssid_len;
    s907x_sta_init.password = wf_info->pwd;
    s907x_sta_init.password_len = wf_info->pwd_len;
    USER_DBG("s907x_sta_init.password_len = %d\n", s907x_sta_init.password_len);
    
    //only ssid password, try to scan target ssid
    USER_DBG("ret = %d\n",ret);
    ret =  wlan_connect_normal(&s907x_sta_init);
    USER_DBG("ret = %d\n",ret);
    if(!ret){
        dhcpc_start(0, 0);
        USER_DBG("dhcp start!\n");
    }
    Z_DEBUG();
    return ret;
}

static int wlan_set_ap(wifi_info_t *wf_info)
{
    //int security;
    int ret = HAL_OK;
    int id;
    int mode;
    struct ip_addr ipaddr;
    struct ip_addr netmask;
    struct ip_addr gw;	
    s907x_ap_init_t ap_init = {0};
    
    ap_init.ssid = wf_info->pwd;
    ap_init.ssid_len = wf_info->ssid_len;
    ap_init.password = wf_info->pwd;
    ap_init.password_len = (sizeof(wf_info->pwd)/sizeof(wf_info->pwd[0]));
    
    //set default none 
    ap_init.security = S907X_SECURITY_NONE;
    ap_init.channel = 5;// chn5 default
    //ap_init.is_hidded_ssid = 0; 
   
    mode = s907x_wlan_get_mode();
    if(mode != S907X_MODE_STA_AP) {
        id = S907X_DEV0_ID;
    } else {
        id = S907X_DEV1_ID;
    }
    
    //country code default
    //phymode default
    ret = s907x_wlan_start_ap(&ap_init);
    USER_DBG("start ap ret:%d\n",ret);
    if(!ret){
        //static ip
        IP4_ADDR(&ipaddr, AP_IP_ADDR0 ,AP_IP_ADDR1 , AP_IP_ADDR2 , AP_IP_ADDR3 );
        IP4_ADDR(&netmask, AP_NETMASK_ADDR0, AP_NETMASK_ADDR1, AP_NETMASK_ADDR2, AP_NETMASK_ADDR3);
        IP4_ADDR(&gw, AP_GW_ADDR0, AP_GW_ADDR1, AP_GW_ADDR2, AP_GW_ADDR3);
        netif_set_addr(LwIP_GetNetif(id), &ipaddr, &netmask,&gw);
    }
    dhcps_init(LwIP_GetNetif(id));
    if(ret == HAL_OK){
      USER_DBG("start ap success.\n");
    }
    return ret;
}


int s907x_wlan_start(u8 wf_m, wifi_info_t *wf_info)
{
    int ret = HAL_OK;
    u8 mode;
    if(wf_m < ZG_STA_MODE || wf_m > ZG_AP_MODE){
        ret = HAL_ERROR;
        goto exit;
    } 
    
    mode = s907x_wlan_get_mode();
    if(wf_m & ZG_AP_MODE){
        if(S907X_MODE_STA == mode)
            mode = S907X_MODE_AP;      
    }
    
    if(wf_m & ZG_STA_MODE){
        if(S907X_MODE_AP == mode)
            mode = S907X_MODE_STA;
    }
    s907x_wlan_off(); 
    wl_os_mdelay(50);
    s907x_wlan_on(mode);
    
    if(mode & S907X_MODE_AP){
        ret = wlan_set_ap(wf_info);
    }
    
    if(mode & S907X_MODE_STA){
       ret = wlan_set_sta(wf_info);
    }
    
exit:
  
    return ret;
    
}

static void get_mac_netif(u8 id, u8* mac)
{
    s907x_wlan_get_mac_address(id, mac);
}

int wlan_get_mac_addr(u8 netif_id, u8* mac, int mac_len)
{
    int ret = -1;
    
    if(netif_id < S907X_DEV0_ID || netif_id > S907X_DEV1_ID){
      goto exit;
    }

    get_mac_netif(netif_id, mac);
    ret = 0;
  
exit:

    return ret;
    
}






#endif



