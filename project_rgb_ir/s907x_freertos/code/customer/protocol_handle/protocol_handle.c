#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sys/ZG_system.h"
/**
* @brief Handling Interaction Protocol
* @param data
* @note  
* @retval none
*/
void protocol_data_parser(uint8_t *buff ,uint16_t len,protocol_event_t type)
{
  int crc_check = PARSER_SUCCESS;
  uint8_t sendbuff[88] = {0};
  unsigned char *send_data = (unsigned char*)malloc(150);
  int send_len = 0;

  colort_struct_t color;

  struct rtc_time Real_time;
  memset(&color,0,sizeof(colort_struct_t));

  if(type ==  CLOUD_DATA_EVENT){

  	crc_check = protocol_head_parser(buff,&len);
	set_report_reson(CLOUD_CONTROL);
  }else{
		
    set_report_reson(LOCAL_CONTROL);
  }

  if(crc_check == PARSER_SUCCESS){

	  switch (buff[0]){

		case COMMAND_TIME_SET:
		/**
		 * 校对时间命令：
		 * 发：	【0X10】＋【0x14】＋【年十个位】＋【月】＋【日】＋【时】＋【分】＋【秒】＋【周】＋【保留位：0x0】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：12)				
		 */	

			Real_time.tm_year = buff[2] + 2000;
			Real_time.tm_mon = buff[3];
			Real_time.tm_mday = buff[4];
			Real_time.tm_hour = buff[5];
			Real_time.tm_min = buff[6];
			Real_time.tm_sec = buff[7];
			Real_time.tm_wday = buff[8];
			RTC_time_Conf(&Real_time);

			break;

		case COMMAND_TIME_QUERY:
		/**
		 * 查询时间命令：
		 * 发：	【0X11】＋【0X1A】＋【0X1B】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：5)			
		 * 返回：	【0xF0远程，0x0F本地】+【0X11】＋【0x14】＋【年十个位】＋【月】＋【日】＋【时】＋【分】＋【秒】＋【周】＋【保留位：0x0】＋【校验码】(命令长度：12)			
		 */ 
	
	        get_RTC_time(&Real_time);//锟斤拷取RTC
	        sendbuff[0] = buff[3];
	        sendbuff[1] = 0x11;
	        sendbuff[2] = 0x14;
	        sendbuff[3] = Real_time.tm_year - 2000;
	        sendbuff[4] = Real_time.tm_mon;
	        sendbuff[5] = Real_time.tm_mday;
	        sendbuff[6] = Real_time.tm_hour;
	        sendbuff[7] = Real_time.tm_min;
	        sendbuff[8] = Real_time.tm_sec;
	        sendbuff[9] = Real_time.tm_wday;
	        sendbuff[10] = 0x0;
	        sendbuff[11] = data_checkSum(sendbuff,11);

	        if(buff[3] ==  CLOUD_QUERY){

	          send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,12);
	          tcp_Client_send(send_data,len);
	        }else{

	           tcp_server_multi_send(sendbuff,12);
	        }

	   break;

		case COMMAND_CLOCK_SET:
		/**
		 * 设置定时内容命令：
		 * 【0X21】＋                                                                                                                                                              
		 *【1时段定时开关】＋【1时段年十个位】＋【1时段月】＋【1时段日】+【1时段时】＋【1时段分】＋【1时段秒】+【1时段周】＋【1时段模式】＋【1时段32bit RGBW值】＋【1时段开关】＋
		 *【2时段定时开关】＋【2时段年十个位】＋【2时段月】＋【2时段日】+【2时段时】＋【2时段分】＋【2时段秒】+【2时段周】＋【2时段模式】＋【2时段32bit RGBW值】＋【2时段开关】＋
		 *【3时段定时开关】＋【3时段年十个位】＋【3时段月】＋【1时段日】+【3时段时】＋【3时段分】＋【3时段秒】+【3时段周】＋【3时段模式】＋【3时段32bit RGBW值】＋【3时段开关】＋
		 *【4时段定时开关】＋【4时段年十个位】＋【4时段月】＋【4时段日】+【4时段时】＋【4时段分】＋【4时段秒】+【4时段周】＋【4时段模式】＋【4时段32bit RGBW值】＋【4时段开关】＋
		 *【5时段定时开关】＋【5时段年十个位】＋【5时段月】＋【5时段日】+【5时段时】＋【5时段分】＋【5时段秒】+【5时段周】＋【5时段模式】＋【5时段32bit RGBW值】＋【5时段开关】＋   
		 *【6时段定时开关】＋【6时段年十个位】＋【6时段月】＋【6时段日】+【6时段时】＋【6时段分】＋【6时段秒】+【6时段周】＋【6时段模式】＋【6时段32bit RGBW值】＋【6时段开关】＋
		 *【保留位：0x0】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：88)
		 * 
		 * 返回：	【0xF0远程，0x0F本地】+ 【0X21】+ 【0x00】＋【校验码】(命令长度：4)			
	* 备注：定时内容设置命令(下发命令)          RGBW值：当模式为 0x61时使用，当为动态模式时，第一字节为速度值，范围：0x01-0x1f；定时开关：当为0xF0时定时生效，0x0F时定时失效； 开关：当为0xF0时为开，0x0F时为关； 周：设置循环定时，bit0为空，bit1-bit7分别对应周一到周日，bit7为周日；			
	*/ 
        timer_clock_Conf(buff,88);
        sendbuff[0] = buff[86];
        sendbuff[1] = 0x21;
        sendbuff[2] = 0x00;
        sendbuff[3] = data_checkSum(sendbuff,3);

        if(buff[86] == 0xf0){

          send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
          tcp_Client_send(send_data,len);

        }else{
           tcp_server_multi_send(sendbuff,4);
        }
		case COMMAND_CLOCK_QUERY:
		/**
		 * 查询定时内容命令：
		 * 发：	【0X22】＋【0X2A】＋【0X2B】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：5)	
		 *返回：	"【0xF0远程，0x0F本地】+【0X22】＋                                                                                                                                                              
		 *【1时段定时开关】＋【1时段年十个位】＋【1时段月】＋【1时段日】+【1时段时】＋【1时段分】＋【1时段秒】+【1时段周】＋【1时段模式】＋【1时段32bit RGBW值】＋【1时段开关】＋
		 *【2时段定时开关】＋【2时段年十个位】＋【2时段月】＋【2时段日】+【2时段时】＋【2时段分】＋【2时段秒】+【2时段周】＋【2时段模式】＋【2时段32bit RGBW值】＋【2时段开关】＋
		 *【3时段定时开关】＋【3时段年十个位】＋【3时段月】＋【1时段日】+【3时段时】＋【3时段分】＋【3时段秒】+【3时段周】＋【3时段模式】＋【3时段32bit RGBW值】＋【3时段开关】＋
		 *【4时段定时开关】＋【4时段年十个位】＋【4时段月】＋【4时段日】+【4时段时】＋【4时段分】＋【4时段秒】+【4时段周】＋【4时段模式】＋【4时段32bit RGBW值】＋【4时段开关】＋
		 *【5时段定时开关】＋【5时段年十个位】＋【5时段月】＋【5时段日】+【5时段时】＋【5时段分】＋【5时段秒】+【5时段周】＋【5时段模式】＋【5时段32bit RGBW值】＋【5时段开关】＋   
		 *【6时段定时开关】＋【6时段年十个位】＋【6时段月】＋【6时段日】+【6时段时】＋【6时段分】＋【6时段秒】+【6时段周】＋【6时段模式】＋【6时段32bit RGBW值】＋【6时段开关】＋
		 *【保留位：0x0】＋【校验码】(命令长度：88)"	
	*/
	        sendbuff[0] = buff[3];
	        sendbuff[1] = 0x22;
	        memcpy(sendbuff + 2,check_timer_clock_data() + 1,84);
	        sendbuff[86] = 0x00;
	        sendbuff[87] = data_checkSum(sendbuff,87);

	        if(buff[3] == 0xf0){

	           len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,88);
	           tcp_Client_send(send_data,len);
	        }else{

	           tcp_server_multi_send(sendbuff,88);
	        }
			 
	 break;

		case COMMAND_COLOR_SET:
		/**
		 * 设置颜色、暖白值
		 * 发：	【0X31】＋【8bit红色数据】＋【8bit绿色数据】＋【8bit蓝色数据】＋【8bit暖白数据】＋【8bit冷白数据】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：8)	
	* 返回：	"如果是本地(0x0F)：无返回
	* 如果是远程(0xF0)：【0xF0远程】+ 【0X31】＋【0x00】＋【校验码】"	
		 *"备注：手机发出，用于显示静态颜色的命令，数据范围：00-0xff，当为0时，PWM为0%，当为0XFF时，对应颜色PWM为100%；
	* 收到此命令模式为0x61；"	
	*/
			set_led_mode(MODE_STATIC);
			color.music_flag = 0;
			color.R = buff[1];
			color.G = buff[2];
			color.B = buff[3];
			Set_color(&color);

			if(buff[6] == CLOUD_QUERY) {

				sendbuff[0] = buff[3];
				sendbuff[1] = 0x31;
				sendbuff[2] = 0x00;
				sendbuff[3] = data_checkSum(sendbuff,3);
				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
				tcp_Client_send(send_data,send_len);    
			}
				
		    break;		

		case COMMAND_MUSIC_SET_FAST:
		/**
		 * 设置颜色、 暖白值 （For 音乐模式，不保存）
		 * 发：	【0X41】＋【8bit红色数据】＋【8bit绿色数据】＋【8bit蓝色数据】＋【8bit暖白数据】＋【8bit冷白数据】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：8)	
	* 返回：	无返回
		 *"备注：收到此命令模式为0x62；"	
	*/

				set_led_mode(MODE_MUSIC);
				color.music_flag = 1;
				color.R = buff[1];
				color.G = buff[2];
				color.B = buff[3];
				Set_color(&color);

		    break;		

		case COMMAND_MUSIC_SET_SLOW:
		/**
		 * 设置颜色、 暖白值 （For 音乐模式，不保存）慢变，即前一个颜色到另一个颜色是渐变过度的
		 * 发：	【0X42】＋【8bit红色数据】＋【8bit绿色数据】＋【8bit蓝色数据】＋【8bit暖白数据】＋【8bit冷白数据】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：8)	
	* 返回：	无返回
		 *"备注：收到此命令模式为0x62；"	
	*/
			set_led_mode(MODE_MUSIC);

			color.R = buff[1];
			color.G = buff[2];
			color.B = buff[3];
			Set_color(&color);
			break;		
		case COMMAND_USERDEFINE_SET:
		/**
		 * 设置自定义模式的命令：
		 * 【0X51】＋【第1点32bit色度值】＋【第2点32bit色度值】＋【第3点32bit色度值】＋
	* 【第4点32bit色度值】＋【第5点32bit色度值】＋【第6点32bit色度值】＋
	* 【第7点32bit色度值】＋【第8点32bit色度值】＋【第9点32bit色度值】＋
	* 【第10点32bit色度值】＋【第11点32bit色度值】＋【第12点32bit色度值】＋
	* 【第13点32bit色度值】＋【第14点32bit色度值】＋【第15点32bit色度值】＋
	* 【第16点32bit色度值】＋【8bit速度值】＋【8bit CHANGING模式值】＋
	* 【0XFF】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：70)
		 * 
		 * 	返回：	"如果是本地(0x0F)：无返回
	*  如果是远程(0xF0)：【0xF0远程】+ 【0X51】＋【0x00】+【校验码】"	
		 * 
	*  备注：自定义模式数据命令(下发命令),RGB中不能为：0x01，0x02，0x03，否则当成结束
	*  收到此命令模式为0x60；
		 */ 

			if(get_led_switch() == MODE_OFF){

				 set_led_switch(MODE_ON);
			}
			set_led_mode(MODE_USER);
			custom_mode_data_conf(buff + 1);   

			if(buff[68] == CLOUD_QUERY) {

				sendbuff[0] = buff[3];
				sendbuff[1] = 0x51;
				sendbuff[2] = 0x00;
				sendbuff[3] = data_checkSum(sendbuff,3);
				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
				tcp_Client_send(send_data,send_len);   
			}
				
		    break;

		case COMMAND_USERDEFINE_QUERY:
	/**
		 * 查询自定义模式内容的命令：
		 * 【0xF0远程，0x0F本地】+【【0X52】＋【第1点32bit色度值】＋【第2点32bit色度值】＋【第3点32bit色度值】＋
	* 【第4点32bit色度值】＋【第5点32bit色度值】＋【第6点32bit色度值】＋
	* 【第7点32bit色度值】＋【第8点32bit色度值】＋【第9点32bit色度值】＋
	* 【第10点32bit色度值】＋【第11点32bit色度值】＋【第12点32bit色度值】＋
	* 【第13点32bit色度值】＋【第14点32bit色度值】＋【第15点32bit色度值】＋
	* 【第16点32bit色度值】＋【8bit速度值】＋【8bit CHANGING模式值】＋
	* 【0XFF】＋【校验码】(命令长度：70) 
		 */

			sendbuff[0] = buff[3];
			sendbuff[1] = 0x52;
			memcpy(&sendbuff[2],get_custom_mode_data() + 1,66);
			sendbuff[68] = 0xff;
			sendbuff[69] = data_checkSum(sendbuff,69);

			if(buff[3] == 0xf0){

				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,70);
				tcp_Client_send(send_data,send_len);             
			}else{

			    tcp_server_multi_send(sendbuff,70);
			}
			 
		    break;	

		case COMMAND_DYNAMIC_SET:
		/**
		 * 发：	【0x61】＋【8bit模式值】＋【8bit速度值】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：5)	
		 * 返回：	"如果是本地(0x0F)：无返回
		 * 如果是远程(0xF0)：【0xF0远程】+ 【0x61】＋【0x00】+【校验码】"	
		 * 备注: 速度值 0x01--0x1F	
		 */ 

		  if(get_led_switch() == MODE_OFF){

				 set_led_switch(MODE_ON);
			}
			set_led_mode(buff[1]);
			set_led_speed(buff[2]);
			if(buff[3] == CLOUD_QUERY) {

				sendbuff[0] = buff[3];
				sendbuff[1] = 0x61;
				sendbuff[2] = 0x00;
				sendbuff[3] = data_checkSum(sendbuff,3);
				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
				tcp_Client_send(send_data,send_len);   
			}
				
		 	  break;
	case COMMAND_SET_SORT:

			set_color_output_sort(buff[2]);
			sendbuff[0] = buff[3];
			sendbuff[1] = buff[0];
			sendbuff[2] = 0x00;
			sendbuff[3] = data_checkSum(sendbuff,3);
			if(buff[3] == 0xf0){

				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
				tcp_Client_send(send_data,send_len);   
			}else{
 
		        tcp_server_multi_send(sendbuff,4);
			}
	      	break;
		case COMMAND_SWTICH:	
	/**
		 * 设置键值（开关命令）命令：
		 * 发：	【0X71】＋【8bit键值】＋【0xF0远程，0x0F本地】＋【校验码】(命令长度：4)	
	* 返回：	【0xF0远程，0x0F本地】+ 【0X71】＋【开关状态值】＋【校验码】	
	* 备注：键值0x23代表开，0x24代表关； 0x22代表切换，即原来是开即变关，关变开；
	* 需要返回“【开关状态值】
		 */ 

			set_led_switch(buff[1]);
			sendbuff[0] = buff[2];
			sendbuff[1] = 0x71;
			sendbuff[2] = get_led_switch();
			sendbuff[3] = data_checkSum(sendbuff,3);
			if(buff[2] == 0xf0){

				send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,4);
				tcp_Client_send(send_data,send_len);         
			}else{

				tcp_server_multi_send(sendbuff,4);
			}
			
		    break;

		case COMMAND_DEVICE_QUERY:
				/*
				* 设备状态查询命令：	
				*发：	【0X81】+【0X8A】+【0X8B】+【校验码】(命令长度：4)	
				*返回：	【0X81】+【8bit设备名】+【8bit开/关机】+【8bit模式值】+【8bit运行/暂停状态】+ 【8bit速度值】＋【8bit红色数据】＋【8bit绿色数据】＋【8bit蓝色数据】+  【8bit暖白数据】+【版本号】+【8bit冷白数据】+【保留位：0X0】+【校验码】(命令长度：14)	
				*备注：当模块收到设备查询命令后，模块应答此命令，
				*【8bit开/关机】：此刻开机回复0x23，关机回复0x24；
				*【8bit运行/暂停状态】表示当前的运行状态,0x20：运行状态，0x21：暂停状态,在这里该项暂时没有作用；
				*【8bit速度值】表示动态模式速度值，范围：0x01-0x1f，0x01为最快；
				*/

				sendbuff[0] = 0x81;
				sendbuff[1] = ZG_get_device_code();
				sendbuff[2] = get_led_switch();
				sendbuff[3] = get_led_mode();
				sendbuff[4] = get_color_output_sort() << 4 | 0x03;
				sendbuff[5] = get_led_speed();
				sendbuff[6] = get_red_value();
				sendbuff[7] = get_green_value();
				sendbuff[8] = get_blue_value();
				sendbuff[9] = get_warm_value();
				sendbuff[10] = ZG_get_device_protocol_version();
				sendbuff[11] = get_cold_value();
				sendbuff[12] = get_Color_flag();
				sendbuff[13] = data_checkSum(sendbuff,13);
				if(type ==  CLOUD_DATA_EVENT){

				  send_len = reponse_data_pack(PACK_LED_PROTOCOL,BYTE_FORMAT,sendbuff,send_data,14);
				  tcp_Client_send(send_data,len);

				}
				tcp_server_multi_send(sendbuff,14);
				break;	
		
			default:
				break;
		}		
   }

   free(send_data);
}