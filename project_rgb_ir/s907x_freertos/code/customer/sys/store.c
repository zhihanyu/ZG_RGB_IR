#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "image/fdcm.h"
#include "ZG_system.h"
#include "kernel/FreeRTOS/task.h"

#define USER_DATA_ADDRESS 1024 * 1024
#define USER_DATA_SIZE    4096 
#define SOTRE_SIZE        400

static uint8_t store_dat[SOTRE_SIZE];
static fdcm_handle_t *g_fdcm_hdl;

/*
*| 0  ~ 3  | 4~5      |    6     |       7      |    8    |    9     |     10    |    11    |   12 ~ 13   |  14 ~ 15    |  16 ~  81(66)   | 82 ~ 171(90) |
*| ota flag| wifi mode| Cloud Sw |  reset count | led sw  | led mode | led speed | led sort | remote addr | server port | led custom data |    timer     |
*
*| 172 ~ 236(64) |   237 ~ 252(16) | 253 ~ 285(33) | 286 ~ 350  |    351    |   352   |   353    |    354   |    355   |    356          |   357 ~ 359 | 360 ~ 361
*|   host name   |      aes key    |      SSID     |   password | printf en |  st1_h  |   st1_l  |   st2_h  |   st2_l  |    store number |   RGB       |  CCT
*/


int  ZG_data_save(store_type_t type,unsigned char *dat)
{
  int return_type = ESP_OK;

   switch(type){

       case WIFI_MODE_STORE :
          store_dat[4] = *dat;
          printf("store:wifi mode %d\n",*dat);
          break;
       case COLOR_STORE:
          memcpy(store_dat + 357,dat,3);
          printf("store:R G B %d %d %d\n",dat[0],dat[1],dat[2]);
          break;
       case CCT_STORE:
          memcpy(store_dat + 360,dat,2);
          printf("store:W %d C %d\n",dat[0],dat[1]);
          break;
       case CLOUD_SWITCH_STORE:
          memcpy(store_dat + 6,dat,1);
          printf("store:cloud switch %X\n",*dat);
          break;
       case RESTORE_STORE:
          memcpy(store_dat + 7,dat,1);
          printf("store:factory count %d\n",*dat);
          break;
       case LED_SWITCH_STORE:
          memcpy(store_dat + 8,dat,1);
          printf("store:led switch %X\n",*dat);
          break;
       case LED_MODE_STORE:
          memcpy(store_dat + 9,dat,1);
          printf("store:led mode %X\n",*dat);
          break;
       case LED_SPEED_STORE:
          memcpy(store_dat + 10,dat,1);
          printf("store:led speed %X\n",*dat);    
          break;
       case LED_SORT_STORE:
          memcpy(store_dat + 11,dat,1);
          printf("store:led wring type %X\n",*dat);   
          break;
       case REMOTE_ADDR_STORE:
          memcpy(store_dat + 12,dat,2);
          printf("store:led remote %X %d\n",dat[0],dat[1]);   
          break;
       case PORT_STORE:
          memcpy(store_dat + 14,dat,2);
          printf("store:port %X %d\n",dat[0],dat[1]);   
          break;
       case LED_CUSTOM_MODE_STORE:
          memcpy(store_dat + 16,dat,66);
         
          break;
       case CLOCK_STORE:
          memcpy(store_dat + 82,dat,90);

          break;
       case HOST_STORE :
           if(dat[0] > 64){
              return -1;
           }
           store_dat[172] = dat[0];
           memcpy(store_dat + 173,dat+1,dat[0]);
         
           /*分配64字符*/
         break;
       case AES_KEY_STORE :
           memcpy(store_dat + 237,dat,16);
           
           break;
       case DEV_SSID_STORE :
           if(dat[0] > 32){
              return -1;
           }
           store_dat[253] = dat[0];
           memcpy(store_dat + 254,dat+1,dat[0]);
     
           /*分配33字符*/
           break;
       case DEV_PWD_STORE :
           if(dat[0] > 64){
              return -1;
           }
           store_dat[286] = dat[0];
           memcpy(store_dat + 287,dat+1,dat[0]);
    
           /*分配65字符*/
           break;
      case PRINTF_EN_STORE:

           store_dat[351] = dat[0];
           break;
      case ST1_STORE:
           store_dat[352] = dat[0];
           store_dat[353] = dat[1];
      
           break; 
      case ST2_STORE:
           store_dat[354] = dat[0];
           store_dat[355] = dat[1];
           break; 
   }
   
  return return_type;
}

int ZG_data_read(store_type_t type,unsigned char *dat)
{
  int  return_type = ESP_OK;

   switch(type){
       case WIFI_MODE_STORE :
          dat[0] = store_dat[4];
          printf("system read:wifi mode %d\n",dat[0]);
          break;
       case COLOR_STORE:
           dat[0] = store_dat[357];
           dat[1] = store_dat[358];
           dat[2] = store_dat[359];
           printf("system read:R G B %d %d %d\n",dat[0],dat[1],dat[2]);
          break;
       case CCT_STORE:
          dat[0] = store_dat[360];
          dat[1] = store_dat[361];
          printf("system read:W %d C %d\n",dat[0],dat[1]);
          break;
       case CLOUD_SWITCH_STORE:
          dat[0] = store_dat[6];
          printf("system read:cloud switch %X\n",dat[0]);
          break;
       case RESTORE_STORE:
          dat[0] = store_dat[7];
          printf("system read:factory count %d\n",dat[0]);
          break;
       case LED_SWITCH_STORE:
          dat[0] = store_dat[8];
          printf("system read:led switch:%x\n",dat[0]);
          break;
       case LED_MODE_STORE:
          dat[0] = store_dat[9];
          printf("system read:led mode:%x\n",dat[0]);
          break;
       case LED_SPEED_STORE:
          dat[0] = store_dat[10];
          printf("system read:led speed:%x\n",dat[0]);
          break;
       case LED_SORT_STORE:
          dat[0] = store_dat[11];
          printf("system read:led sort:%x\n",dat[0]);
          break;
       case REMOTE_ADDR_STORE:
          dat[0] = store_dat[12];
          dat[1] = store_dat[13];
          printf("system read:remote adrress:%x\n",dat[0] << 8 | dat[1]);
          break;
       case PORT_STORE:
          dat[0] = store_dat[14];
          dat[1] = store_dat[15];
          printf("system read:port:%x\n",dat[0] << 8 | dat[1]);
          break;
       case LED_CUSTOM_MODE_STORE:
          memcpy(dat,store_dat + 16,66);
          break;
       case CLOCK_STORE:
          memcpy(dat,store_dat + 82,90);
          break;
       case HOST_STORE :
          dat[0] = store_dat[172];
          if(dat[0] > 64){
            
             return_type = ESP_FAIL;
          }else{
             memcpy(dat + 1,store_dat + 173,dat[0]);
          }
           /*分配64字符*/
         break;
       case AES_KEY_STORE :
          
          memcpy(dat,store_dat + 237,16);
          break;
       case DEV_SSID_STORE :
           dat[0] = store_dat[253];
           if(dat[0] > 32){

             return_type = ESP_FAIL; 
           }else{
             memcpy(dat + 1,store_dat + 254,dat[0]);
           }
           /*分配33字符*/
           break;
       case DEV_PWD_STORE :
           dat[0] = store_dat[286];
           if(dat[0] > 64){

             return_type = ESP_FAIL; 
           }else{
             memcpy(dat + 1,store_dat + 287,dat[0]);
           }
        
           /*分配65字符*/
           break;
      case PRINTF_EN_STORE:
           dat[0] = store_dat[351];
           break;
      case ST1_STORE:
           dat[0] = store_dat[352];
           dat[1] = store_dat[353];
           printf("system read:st1: %d\n",dat[0] << 8 | dat[1] );
           break; 
      case ST2_STORE:
           dat[0] = store_dat[354];
           dat[1] = store_dat[355];
           printf("system read:%d\n",dat[0] << 8 | dat[1] );
           break; 
      default :
           return_type = ESP_FAIL;
           break;
   }
   
  return return_type;
}


static int store_num;
void ZG_store_fast()
{
  if (fdcm_write(g_fdcm_hdl, store_dat, SOTRE_SIZE) != SOTRE_SIZE) {
         
      printf("fast store write fail\n");  
  }
}


static void store_task(void *arg)
{
  unsigned char *buf = (unsigned char *)malloc(SOTRE_SIZE);
  memcpy(buf,store_dat,SOTRE_SIZE);
  vTaskDelay(5000 / portTICK_PERIOD_MS); 
  printf("store task start!!!\n");
  while(1){

     vTaskDelay(3000 / portTICK_PERIOD_MS); 
     if(memcmp(store_dat,buf,SOTRE_SIZE) != 0){

        store_num++;
        store_dat[356] = store_num;
        printf("\n|store count -> %d|\n\n",store_num);
        memcpy(buf,store_dat,SOTRE_SIZE);
        if (fdcm_write(g_fdcm_hdl, store_dat, SOTRE_SIZE) != SOTRE_SIZE) {
               
            printf("store write fail\n");  
        }
     }
  }
  free(buf);
  vTaskDelete(NULL);
}

void store_init()
{
  g_fdcm_hdl = fdcm_open(0, USER_DATA_ADDRESS, USER_DATA_SIZE);
  if(g_fdcm_hdl == NULL){

    printf("store init fail\n");
  }else{
    if (fdcm_read(g_fdcm_hdl, store_dat, SOTRE_SIZE) != SOTRE_SIZE) {

       printf("store read fail\n");
    }
     xTaskCreate(&store_task, "G_store_task", 512, NULL,tskIDLE_PRIORITY + 2, NULL);
  }

}