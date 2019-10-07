#ifndef FOTA_H_
#define FOTA_H_


#ifdef __cplusplus
extern "C" {
#endif

int fota_conf(char *host,int host_len,uint16_t port,char *path,int path_len,char *code,int code_len);
int ota_start();

#ifdef __cplusplus
}
#endif



#endif 