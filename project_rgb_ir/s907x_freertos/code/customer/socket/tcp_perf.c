
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kernel/FreeRTOS/FreeRTOS.h"
#include "kernel/FreeRTOS/task.h"
#include "kernel/FreeRTOS/event_groups.h"
#include "kernel/FreeRTOS/queue.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#include "sys/ZG_system.h"

static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;

static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;
static int client_socket[FD_SETSIZE]={0};
static int conn_amount;

int socket_keepalive(int fd, int keep_idle, int keep_interval, int keep_count)
{
    if(fd==-1){
     return -1;
    }
    int ret = 0;
    int keep_alive = true;

    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
    if(ret<0){
        return ret;
    }

    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
    if(ret<0){
        return ret;
    }

    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
    if(ret<0){
        return ret;
    }

    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
    if(ret<0){
        return ret;
    }
    return 0;
}

/**
 * @brief 叠包解析器
 * @author hings
 * @note 
 * @param[in]  data  : 输入数据，
 * @param[in] len  : length of data
 * @param[in] preset_len  : 设置解析的包长 1~64
 * @return none
 */
static void overlap_data_parser(uint8_t * data,int len,int preset_len)
{
   
    if(len > preset_len && len % preset_len == 0 && len <= 4 * preset_len){

        uint8_t parser_buf[64] = {0};
        int len_t = len;
        while(len_t > 0){
        
            memcpy(parser_buf,data + len - len_t,preset_len);
            len_t -= preset_len;
            protocol_data_parser(parser_buf,preset_len,LOCAL_DATA_EVENT);

        }
        
    }else{
        
        protocol_data_parser(data + len - preset_len,preset_len,LOCAL_DATA_EVENT);
    }
}


void socket_client_destroy(int index)
{

  close(client_socket[index]);
  printf("rev failed:clean fd: %d\n",client_socket[index]);

  int DeleteIndex = -1;
  for(int i = 0;i < conn_amount;i++){

      if(client_socket[index] == client_socket[i]){

          DeleteIndex = i;
          break;
      }   
  }
  if(DeleteIndex == -1){

      printf("list find failed！\n");
  }else{
      /*delete date in list*/
      for(int i = DeleteIndex;i < conn_amount;i++){

          client_socket[i] = client_socket[i + 1];
      }
  }

  conn_amount--;

  printf("client list %d:\n",conn_amount);
  for(int listi = 0;listi < conn_amount;listi++){

      printf("[%d:%d]\n",listi,client_socket[listi]);
  }
  printf("\n");  
}

static int create_tcp_server(int port)
{
    uint8_t *ip;
    int max_fd   =-1;
    int opt_val  = 1;
    int fd;
    int len = 0;
    uint8_t *recv_data = NULL ;
    uint8_t *send_data = NULL;

    send_data = (uint8_t*)malloc(data_PKTSIZE);
    recv_data =  (uint8_t*)malloc(data_PKTSIZE);

    fd_set readset;
    fd_set writeset;

    struct timeval select_timeout = {0, 50000};

    server_socket = socket(AF_INET, SOCK_STREAM, 0);//初始化socket
    if (server_socket < 0) {
        
      return -1;
    }
    server_addr.sin_family = AF_INET;//地址结构的协议族
    server_addr.sin_port = htons(port);//
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//机器运行的ip
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
       
        close(server_socket);
        return -1;
    }
    if (listen(server_socket, 5) < 0) {
       
        close(server_socket);
        return -1;
    }
    max_fd = server_socket;

    while(1){

        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_SET(server_socket, &readset);

        for(fd = 0;fd < FD_SETSIZE;fd++){

            if(client_socket[fd] > 0){

                 FD_SET(client_socket[fd], &readset);
            }
        }
        if (select(max_fd + 1, &readset, (fd_set *)0, NULL, &select_timeout) <= 0) {//无文件可读或无文件可写，则执行continue
            continue;
        }

        if(FD_ISSET(server_socket,&readset)){

            connect_socket = accept(server_socket, (struct sockaddr*)&client_addr, &socklen);

            if (connect_socket < 0) {
                
                close(connect_socket);
                printf("tcp new chil fail\n");
                continue;
            }else{

                max_fd = connect_socket;
                client_socket[conn_amount] = connect_socket;
                socket_keepalive(connect_socket, 50, 5, 3);
                printf("tcp ocket count:%d\n",conn_amount);
                conn_amount++;
                printf("adding client on fd %d\n", connect_socket);
                ip = (uint8_t*)&client_addr.sin_addr;
                printf("insert sockfd: %d, ip: %d:%d:%d:%d, port: %d", connect_socket, ip[0], ip[1], ip[2], ip[3], client_addr.sin_port);
                printf("client list %d:\n",conn_amount);
                for(int listi = 0;listi < conn_amount;listi++){

                    printf("[%d:%d]\n",listi,client_socket[listi]);
                }
                printf("\n");
            }
        }

        for(fd = 0;fd < conn_amount;fd++){

            if(FD_ISSET(client_socket[fd],&readset)){

                memset(recv_data,0,data_PKTSIZE);
                memset(send_data,0,data_PKTSIZE);
                /*receiver data here*/
                len = recv(client_socket[fd], recv_data, data_PKTSIZE, 0);
                printf("rev data length:%d\n",len);
                if(len > 0){
                    
                    protocol_data_parser(recv_data,len,LOCAL_DATA_EVENT);
                }else{

                  socket_client_destroy(fd);
                }
            
            }
        }

    }
    free(send_data);
    free(recv_data);
    return 0;
}
int tcp_server_multi_send(unsigned char *buf,int len)
{
    int slen;

    for(int socket_dix = 0 ; socket_dix < conn_amount; socket_dix++){ //send to all tcp client

        
        slen = send(client_socket[socket_dix], buf, len, 0);
        if(slen > 0){
          printf("send success\n");

        }else{
          printf("send failed\n");
          socket_client_destroy(socket_dix);
        }
    }
    return 0;
}
void tcp_conn(void *arg){

    if(create_tcp_server( TCP_SEVER_PORT) >= 0){

        printf(" TCP server creat success\n");
    }else{
        printf("TCP server creat fail\n");
    }
    vTaskDelete(NULL);
}

void tcp_server_task_init()
{
   xTaskCreate(&tcp_conn, "tcp_conn", 4096, NULL,tskIDLE_PRIORITY + 6, NULL);
}