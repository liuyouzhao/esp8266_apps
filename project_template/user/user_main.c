/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"
#include "user_config.h"
#include "espressif/pwm.h"
#include "gpio.h"
#include <math.h>

/* for udp socket broadcast receiver */
#include "udp_socket.h"


#define HZMAX 4800
#define HZMIN 1600
#define FLEX 5.0f
#define DT 800
/* Circle number =  N1/N2 */
#define N1 40
#define N2 3
#define MMPC 75
#define VTNUM 500

static int s_dt = DT;
static int s_step = N1;
static int s_mmpc = MMPC; ///mm

static int s_wifi_ok = 0;
static int s_lock = 0;
static int s_signal = 0;
static int s_cur_step = 0;
static int s_cur_dir = 0;
static int s_tim[VTNUM] = {0};
static int mm2step(int mm);

static os_timer_t timer;

static void user_uart0_rx_callback(char *buf, int len);
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

static void init_step_motor()
{
    PIN_FUNC_SELECT(GPIO_PIN_REG_12, FUNC_GPIO12);
    PIN_FUNC_SELECT(GPIO_PIN_REG_13, FUNC_GPIO13);
    PIN_FUNC_SELECT(GPIO_PIN_REG_14, FUNC_GPIO14);
    PIN_FUNC_SELECT(GPIO_PIN_REG_15, FUNC_GPIO15);
}

static void calc_vt()
{
    int i = 0;
    float num = (float)VTNUM / 2.0f;
    float cur = 0;
    float delta = (float)(HZMAX - HZMIN);
    for( i = 0; i < VTNUM; i ++)
    {
        cur = (float)HZMIN + delta / (1.0f + expf(-FLEX * (i - num) / num));
        s_tim[i] = (int)(1000 * 1000 / cur / 2.0f);
        printf("%d ", s_tim[i]);
    }
}

/**
 * Fcurrent = Fmin + ( Fmax - Fmin ) / ( 1 + e^(-Flexible*(i - num)/num) )
*/
static void go_steps(int steps, int dir)
{
    int cur = 0;
    int fastest = 1000 * 1000 / HZMAX / 2;
    int tim = 1000 * 1000 / HZMIN / 2;
    int i = 0;

    GPIO_OUTPUT_SET(13, dir);

    for(i = 0; i < 50; i ++)
        os_delay_us(1000 * 10);

    while(i < steps)
    {
        if(i < VTNUM)
        {
            tim = s_tim[i];
        }
        else if(i > steps - VTNUM - 1)
        {
            tim = s_tim[steps - i - 1];
        }

        GPIO_OUTPUT_SET(12, 1);
        os_delay_us(tim);
        GPIO_OUTPUT_SET(12, 0);
        os_delay_us(tim);

        i ++;
    }
    for(i = 0; i < 50; i ++)
        os_delay_us(1000 * 10);
}


static void gpio_proc_task(void *p)
{
    int i = 0;
    init_step_motor();
    calc_vt();
    while(1)
    {
        os_delay_us(1000 * 50);

        if(s_signal)
        {
            s_lock = 1;

            s_signal = 0;
            go_steps(s_cur_step, s_cur_dir);

            s_lock = 0;
        }
    }
}

static void udp_proc_task(void *p)
{
    int i = 0;
    char *buf = 0;
    int len = 0;
    char addr[16];
#if 0
    /* root loop */
    for(;;)
    {
        /* For udp init */
        while(1)
        {
            os_delay_us(1000 * 50);

            if(s_wifi_ok)
            {
                //wifi_conned();
                i = g_udp_socket.init();
                if(i == -1)
                {
                    continue;
                }
                break;
            }
        }

        printf("udp-bc init OK\n");

        g_udp_socket.broad("hi", 2, 8183);

        memset(addr, 0, 16);
        buf = (char*) malloc(32);
        while(1)
        {
            printf("bfr skt recv\n");
            i = g_udp_socket.recv(buf, addr, &len);
            if(i == -1)
            {
                printf("error recv\n");
                break;
            }
            printf("udp recvd:%s %d\n", buf, len);

            user_uart0_rx_callback(buf, len);
        }

        free(buf);
        printf("re-init udp:)\n");
    }
#endif
    while(1)
    {
        os_delay_us(1000 * 50);

        if(s_wifi_ok)
        {
            udp_client_start(user_uart0_rx_callback);
            break;
        }
    }
    
}

/*
 * distance in mm converting to steps number
*/
static int mm2step(int mm)
{
    int c1 = mm / s_mmpc;
    int c2 = mm % s_mmpc;
    
    int st = c1 * s_dt + c2 * s_dt / s_mmpc;
    return st;
}

static void user_uart0_rx_callback(char *buf, int len)
{
    char dist[8] = {0};
    int ndst = 0;

    if(len < 2 || s_lock == 1)
    {
        return;
    }

    if(buf[0] == 'c' && buf[1] == 'k')
    {
        printf("Y\n");
    }
    else if(len >= 3 && buf[0] == 'm' && buf[1] == 'v')
    {
        /* clockwise */
        if(buf[2] != '-')
        {
            memcpy(dist, buf + 2, len - 2);
            ndst = atoi(dist);
            printf("R%d\n", ndst);

            s_cur_step = mm2step(ndst);
            s_cur_dir = 0;

            s_signal = 1;
        }
        else
        {
            memcpy(dist, buf + 3, len - 3);
            ndst = atoi(dist);
            printf("R%d\n", ndst);

            s_cur_step = mm2step(ndst);
            s_cur_dir = 1;

            s_signal = 1;
        }
    }
    else
    {
        /* TODO: implement other useful command, like setting detail */
    }
}

LOCAL void ICACHE_FLASH_ATTR wait_for_connection_ready(uint8 flag)
{
    os_timer_disarm(&timer);
    if(wifi_station_connected()){
        os_printf("connected\n");
        s_wifi_ok = 1;
    } else {
        os_printf("reconnect after 2s\n");
        os_timer_setfn(&timer, (os_timer_func_t *)wait_for_connection_ready, NULL);
        os_timer_arm(&timer, 2000, 0);
    }
}

LOCAL void ICACHE_FLASH_ATTR on_wifi_connect(){
    os_timer_disarm(&timer);
    os_timer_setfn(&timer, (os_timer_func_t *)wait_for_connection_ready, NULL);
    os_timer_arm(&timer, 100, 0);
}

LOCAL void ICACHE_FLASH_ATTR on_wifi_disconnect(uint8_t reason){
    os_printf("disconnect %d\n", reason);
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    uart_init_new();
    uart0_set_rx_callback(user_uart0_rx_callback);
    
    xTaskCreate(gpio_proc_task, "gpio_proc_task", 512, NULL, 2, NULL);
    xTaskCreate(udp_proc_task, "udp_proc_task", 1024, NULL, 2, NULL);

    /*
     * 1K, duty, 1, io_info 
    */
    set_on_station_connect(on_wifi_connect);
    set_on_station_disconnect(on_wifi_disconnect);
    init_esp_wifi();
    stop_wifi_ap();
    start_wifi_station(SSID, PASSWORD);
}

