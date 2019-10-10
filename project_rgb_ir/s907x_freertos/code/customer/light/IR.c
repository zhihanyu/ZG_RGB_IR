#include "IR.h"
#include "sys/ZG_system.h"
#include "FreeRTOS.h"
#include "task.h"
#include "s907x_zg_config.h"

#if ZG_BUILD

#include "hal_gpio.h"
#include "hal_timer.h"

#define IR_GPIO_SET             (7)
#define TIM_10us                (TIM_CAP)
#define TIM_PRESCALER_NONE      (0)
#define TIM_PERIOD_399          (399)
static  timer_hdl_t tim_hdl;








/** Have TIMER0_ID & TIMER1_ID timer **/
#define TIMERID     TIMER0_ID
//#define TIMERID       TIMER1_ID

#define HFCLOCK         40000000        /*External clock frequency (Hz)*/
#define CLK_PRESCALER   400000         /*clock prescale*/
#define COUNT_TIME      1           /*timer count down time (second)*/


static uint32_t t0 = 0;
static int ir_state  =  IR_NEC_STATE_IDLE;
uint8_t ir_repeat =  0;
static uint32_t ir_cmd    =  0;
static int cnt       =  0;
uint8_t repeat_num;
static uint8_t REV_flag_ok=0;
static uint8_t ir_data;
static uint32_t tmr_us;
static xQueueHandle key_queue;
static unsigned int key_addr;
/******************************************************************************
 * FunctionName : nec_code_check
 * Description  : check whether the ir cmd and addr obey the protocol
 * Parameters   : uint32_t nec_code : nec ir code that received
 * Returns      :   true : cmd check ok  ;  false : cmd check fail
*******************************************************************************/
//check if is a legal nec code 
bool nec_code_check(uint32_t nec_code)
{   
  //  uint8_t addr1,addr2;
    uint8_t cmd1,cmd2;
    cmd2 = (nec_code>>24) & 0xff;     
    cmd1 = (nec_code>>16) & 0xff;   
 //   addr2 = (nec_code>>8) & 0xff;   
 //   addr1 = (nec_code>>0) & 0xff;   
    if( (cmd1 == ((~cmd2) & 0xff)) ){
      //  printf("check ok \n\n");
        return true;
    }else{
       // printf("wrong code \n\n");
        return false;
    }
}
static void irq_gpio_ipc(void *arg) {
    
    uint32_t t_h = 0;
    static uint8_t rep_flg;

    t_h = (tmr_us) - t0;
    t0  =  (tmr_us);
  // printf("t %d \n",t_h);

    switch(ir_state){

        case IR_NEC_STATE_IDLE:  
            if(t_h < IR_NEC_TM_PRE_US + IR_NEC_MAGIN_US * 2 && t_h > IR_NEC_TM_PRE_US - IR_NEC_MAGIN_US * 2){

                ir_state = IR_NEC_STATE_CMD;
              //  printf("head\n");
            }else{
              //   printf("head error\n");
            }
            break;
        
        case IR_NEC_STATE_CMD:
            if(t_h < IR_NEC_D1_TM_US + IR_NEC_MAGIN_US && t_h > IR_NEC_D1_TM_US - IR_NEC_MAGIN_US){

                ir_cmd = (ir_cmd >> 1)|( 0x1 << (IR_NEC_BIT_NUM * 4 - 1) );
                cnt++;
            }else if(t_h < IR_NEC_D0_TM_US + IR_NEC_MAGIN_US && t_h > IR_NEC_D0_TM_US - IR_NEC_MAGIN_US){

                ir_cmd = (ir_cmd >> 1)|( 0x0 << (IR_NEC_BIT_NUM * 4 - 1) );
                cnt++;
            }else{

                goto RESET_STATUS;
            }
          // printf("cnt %d\n",cnt);
            if(cnt == IR_NEC_BIT_NUM * 4){
                if(nec_code_check(ir_cmd))
                {
                    REV_flag_ok = 1;
                    ir_data = ir_cmd >> 16;
                    key_addr = (ir_cmd & 0xff) | ((ir_cmd >> 8) & 0xff);
                    xQueueGenericSend(key_queue,&ir_data,0, queueSEND_TO_BACK);
                 //   printf("ir rcvd:%02x %02x %02x %02x \r\n",ir_cmd&0xff,(ir_cmd>>8)&0xff,(ir_cmd>>16)&0xff,(ir_cmd>>24)&0xff);
                    //printf("get ir key:%02x\n",ir_data);
                    ir_state = IR_NEC_STATE_REPEAT;
                    rep_flg = 0;
                }else{

                    goto RESET_STATUS;    
                }
            }
            break;
        case IR_NEC_STATE_REPEAT:
            if(rep_flg == 0){

                if(t_h > IR_NEC_REP_TM1_US  &&  t_h < IR_NEC_REP_TM1_US * 8){

                    rep_flg = 1;
                }else{
                    goto RESET_STATUS;
                }
            }else if(rep_flg == 1){

                if(t_h < IR_NEC_REP_TM1_US + IR_NEC_MAGIN_US && IR_NEC_REP_TM2_US - IR_NEC_MAGIN_US){

                    xQueueGenericSend(key_queue,&ir_data,0, queueSEND_TO_BACK);
                    repeat_num = ir_repeat;
                    ir_repeat++;
                  //  printf ("ir repeat num %d\n",ir_repeat);
                    rep_flg=0;
                }else{

                   goto RESET_STATUS;
                }
            }else{
            }
            break;
        default: break;
        
RESET_STATUS:
        ir_state = IR_NEC_STATE_IDLE;
        cnt = 0;
        ir_cmd = 0;
        ir_repeat = 0;
        rep_flg = 0;
        REV_flag_ok = 0;
        }
}


int get_remote_key(unsigned char *key,int time)
{
   return (xQueueReceive(key_queue, key, time));
}
unsigned int get_remote_addr()
{
    return key_addr;
}

unsigned char GET_IR_Repeat()
{
  
   return ir_repeat;
}

void zg_timer_callback(void *arg)
{
    tmr_us++;
  //  printf(" timer irq: %u\n", tmr_us);
}


//10 us
static hal_status_e timer_init()
{
#if ZG_BUILD

    hal_status_e sta = HAL_OK;
    timer_hdl_t *p_tim = &tim_hdl;
    
    wl_memset(p_tim, 0, sizeof(p_tim)/sizeof(timer_hdl_t));
    
    //timer id set
    p_tim->config.idx = TIM_10us; 
    //no prescaler
    p_tim->config.prescaler = TIM_PRESCALER_NONE;
    //10us
    p_tim->config.period = TIM_PERIOD_399;
    //interrupt enable
    p_tim->config.int_enable = TRUE;
    //set user callback
    p_tim->it.basic_user_cb.func = zg_timer_callback;
    p_tim->it.basic_user_cb.context = p_tim;

    sta = s907x_hal_timer_base_init(p_tim);
    if(HAL_OK != sta){
        USER_DBG("timer base init fail.\n");
        goto exit;
    }

    sta = s907x_hal_timer_start_base(p_tim);
    if(HAL_OK != sta){
        USER_DBG("timer base start fail.\n");
        s907x_hal_timer_base_deinit(p_tim);
        goto exit;
    }
    
exit:

    return sta;
#endif
}

void IR_Init()
{
    key_queue = xQueueCreate(1, 1);
    gpio_init_t init;
    u32 gpio_pin;

    gpio_pin = BIT(IR_GPIO_SET);
    init.mode = GPIO_MODE_INT_FALLING;
    init.pull = GPIO_NOPULL;
    if(HAL_OK != s907x_hal_gpio_init(gpio_pin, &init)){
    USER_DBG("gpio init fail.\n");
    }
    s907x_hal_gpio_it_start(gpio_pin, irq_gpio_ipc, &gpio_pin);  

    timer_init();
}
#endif
