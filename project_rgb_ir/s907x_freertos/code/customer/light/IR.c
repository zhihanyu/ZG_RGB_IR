#include "IR.h"
#include "sys/ZG_system.h"
#include "driver/chip/hal_gpio.h"
#include "kernel/FreeRTOS/queue.h"
#include "driver/chip/hal_timer.h"

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

void timer_callback(void *arg)
{
    tmr_us++;
  //  printf(" timer irq: %u\n", tmr_us);
}


//10 us
static HAL_Status timer_init()
{
    HAL_Status status = HAL_ERROR;
    TIMER_InitParam param;

    param.arg = NULL;
    param.callback = timer_callback;
    param.cfg = HAL_TIMER_MakeInitCfg(TIMER_MODE_REPEAT,        /*timer mode*/
                            TIMER_CLK_SRC_HFCLK,        /*HFCLOCK*/
                            TIMER_CLK_PRESCALER_4);     /*CLK_PRESCALER*/
    param.isEnableIRQ = 1;
    param.period = COUNT_TIME *(HFCLOCK / CLK_PRESCALER);

    status = HAL_TIMER_Init(TIMERID, &param);
    if (status != HAL_OK)
       printf("timer int error %d\n", status);

    return status;
}

void IR_Init()
{

    key_queue = xQueueCreate(1, 1);
    GPIO_InitParam param;
    param.driving = GPIO_DRIVING_LEVEL_1;
    param.mode = GPIOx_Pn_F6_EINT;
    param.pull = GPIO_PULL_NONE;
    HAL_GPIO_Init(GPIO_EINT_PORT, GPIO_EINT_PIN, &param);

    GPIO_IrqParam irq_param;
    irq_param.arg = NULL;
    irq_param.callback = irq_gpio_ipc;
    irq_param.event = GPIO_IRQ_EVT_FALLING_EDGE; 
    HAL_GPIO_EnableIRQ(GPIO_EINT_PORT, GPIO_EINT_PIN, &irq_param);

    timer_init();
    HAL_TIMER_Start(TIMERID);
}
