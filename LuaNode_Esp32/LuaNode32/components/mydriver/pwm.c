/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP32 only, in
 * which case, it is free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "pwm.h"

#include "esp_common.h"
#include "extras/gpio_reg_ext.h"
#include "pin_map.h"
//#include "io_mux_reg.h"

LOCAL int8 pwm_out_io_num[PWM_CHANNEL_NUM_MAX] = {-1, -1, -1, -1, -1, -1};
LOCAL struct pwm_param pwm;
LOCAL uint16 pwm_gpio = 0;
LOCAL uint8 pwm_channel_num = 0;

/******************************************************************************
 * FunctionName : ledc_set_base_hclk
 * Description  : set high_speed channel base clk
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint8 apb_clk_sel: 0-ref_tick, 1-apb_clk
 * Returns      : NONE
*******************************************************************************/
void ledc_set_base_hclk(uint8 timer_sel, uint8 apb_clk_sel)
{
    if (apb_clk_sel)
    {  // choose apb_clk as base clk
        switch (timer_sel)
        {  //
            case 0:
                SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_TICK_ALWAYS_ON_HSTIMER0);
                break;

            case 1:
                SET_PERI_REG_MASK(LEDC_HSTIMER1_CONF, LEDC_TICK_ALWAYS_ON_HSTIMER1);
                break;

            case 2:
                SET_PERI_REG_MASK(LEDC_HSTIMER2_CONF, LEDC_TICK_ALWAYS_ON_HSTIMER2);
                break;

            case 3:
                SET_PERI_REG_MASK(LEDC_HSTIMER3_CONF, LEDC_TICK_ALWAYS_ON_HSTIMER3);
                break;

            default:
                SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_TICK_ALWAYS_ON_HSTIMER0);
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : ledc_set_base_lclk
 * Description  : set low_speed channel base clk
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint8 apb_clk_sel: 0-ref_tick, 1-apb_clk
 * Returns      : NONE
*******************************************************************************/
void ledc_set_base_lclk(uint8 timer_sel, uint8 apb_clk_sel)
{
    if (apb_clk_sel)
    {  // choose apb_clk as base clk
        switch (timer_sel)
        {  //
            case 0:
                SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_TICK_ALWAYS_ON_LSTIMER0);
                break;

            case 1:
                SET_PERI_REG_MASK(LEDC_LSTIMER1_CONF, LEDC_TICK_ALWAYS_ON_LSTIMER1);
                break;

            case 2:
                SET_PERI_REG_MASK(LEDC_LSTIMER2_CONF, LEDC_TICK_ALWAYS_ON_LSTIMER2);
                break;

            case 3:
                SET_PERI_REG_MASK(LEDC_LSTIMER3_CONF, LEDC_TICK_ALWAYS_ON_LSTIMER3);
                break;

            default:
                SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_TICK_ALWAYS_ON_LSTIMER0);
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : ledc_set_hperiod
 * Description  : set high-speed channel
frequency=base_clk_frequency*div_num*(2^timer_lim)/256
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint32 div_num
                                  uint8 timer_lim
 * Returns      : NONE
*******************************************************************************/
void ledc_set_hperiod(uint8 timer_sel, uint32 div_num, uint8 timer_lim)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_BITS(LEDC_HSTIMER0_CONF, LEDC_DIV_NUM_HSTIMER0, div_num, LEDC_DIV_NUM_HSTIMER0_S);
            SET_PERI_REG_BITS(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_LIM, timer_lim, LEDC_HSTIMER0_LIM_S);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_HSTIMER1_CONF, LEDC_DIV_NUM_HSTIMER1, div_num, LEDC_DIV_NUM_HSTIMER1_S);
            SET_PERI_REG_BITS(LEDC_HSTIMER1_CONF, LEDC_HSTIMER1_LIM, timer_lim, LEDC_HSTIMER1_LIM_S);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_HSTIMER2_CONF, LEDC_DIV_NUM_HSTIMER2, div_num, LEDC_DIV_NUM_HSTIMER2_S);
            SET_PERI_REG_BITS(LEDC_HSTIMER2_CONF, LEDC_HSTIMER2_LIM, timer_lim, LEDC_HSTIMER2_LIM_S);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_HSTIMER3_CONF, LEDC_DIV_NUM_HSTIMER3, div_num, LEDC_DIV_NUM_HSTIMER3_S);
            SET_PERI_REG_BITS(LEDC_HSTIMER3_CONF, LEDC_HSTIMER3_LIM, timer_lim, LEDC_HSTIMER3_LIM_S);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_HSTIMER0_CONF, LEDC_DIV_NUM_HSTIMER0, div_num, LEDC_DIV_NUM_HSTIMER0_S);
            SET_PERI_REG_BITS(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_LIM, timer_lim, LEDC_HSTIMER0_LIM_S);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_set_lperiod
 * Description  : set low-speed channel
frequency=base_clk_frequency*div_num*(2^timer_lim)/256
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint32 div_num
                                  uint8 timer_lim
 * Returns      : NONE
*******************************************************************************/
void ledc_set_lperiod(uint8 timer_sel, uint32 div_num, uint8 timer_lim)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_BITS(LEDC_LSTIMER0_CONF, LEDC_DIV_NUM_LSTIMER0, div_num, LEDC_DIV_NUM_LSTIMER0_S);
            SET_PERI_REG_BITS(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_LIM, timer_lim, LEDC_LSTIMER0_LIM_S);
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PARA_UP);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_LSTIMER1_CONF, LEDC_DIV_NUM_LSTIMER1, div_num, LEDC_DIV_NUM_LSTIMER1_S);
            SET_PERI_REG_BITS(LEDC_LSTIMER1_CONF, LEDC_LSTIMER1_LIM, timer_lim, LEDC_LSTIMER1_LIM_S);
            SET_PERI_REG_MASK(LEDC_LSTIMER1_CONF, LEDC_LSTIMER1_PARA_UP);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_LSTIMER2_CONF, LEDC_DIV_NUM_LSTIMER2, div_num, LEDC_DIV_NUM_LSTIMER2_S);
            SET_PERI_REG_BITS(LEDC_LSTIMER2_CONF, LEDC_LSTIMER2_LIM, timer_lim, LEDC_LSTIMER2_LIM_S);
            SET_PERI_REG_MASK(LEDC_LSTIMER2_CONF, LEDC_LSTIMER2_PARA_UP);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_LSTIMER3_CONF, LEDC_DIV_NUM_LSTIMER3, div_num, LEDC_DIV_NUM_LSTIMER3_S);
            SET_PERI_REG_BITS(LEDC_LSTIMER3_CONF, LEDC_LSTIMER3_LIM, timer_lim, LEDC_LSTIMER3_LIM_S);
            SET_PERI_REG_MASK(LEDC_LSTIMER3_CONF, LEDC_LSTIMER3_PARA_UP);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_LSTIMER0_CONF, LEDC_DIV_NUM_LSTIMER0, div_num, LEDC_DIV_NUM_LSTIMER0_S);
            SET_PERI_REG_BITS(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_LIM, timer_lim, LEDC_LSTIMER0_LIM_S);
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PARA_UP);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_set_ltimer
 * Description  : low_speed channel x choose timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint8 chan_num: 8 channels in total,value from
0~7
 * Returns      : NONE
*******************************************************************************/
void ledc_set_ltimer(uint8 chan_num, uint8 timer_sel)
{
    switch (chan_num)
    {  //
        case 0:
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF0, LEDC_TIMER_SEL_LSCH0, timer_sel, LEDC_TIMER_SEL_LSCH0_S);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_LSCH1_CONF0, LEDC_TIMER_SEL_LSCH1, timer_sel, LEDC_TIMER_SEL_LSCH1_S);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_LSCH2_CONF0, LEDC_TIMER_SEL_LSCH2, timer_sel, LEDC_TIMER_SEL_LSCH2_S);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_LSCH3_CONF0, LEDC_TIMER_SEL_LSCH3, timer_sel, LEDC_TIMER_SEL_LSCH3_S);
            break;

        case 4:
            SET_PERI_REG_BITS(LEDC_LSCH4_CONF0, LEDC_TIMER_SEL_LSCH4, timer_sel, LEDC_TIMER_SEL_LSCH4_S);
            break;

        case 5:
            SET_PERI_REG_BITS(LEDC_LSCH5_CONF0, LEDC_TIMER_SEL_LSCH5, timer_sel, LEDC_TIMER_SEL_LSCH5_S);
            break;

        case 6:
            SET_PERI_REG_BITS(LEDC_LSCH6_CONF0, LEDC_TIMER_SEL_LSCH6, timer_sel, LEDC_TIMER_SEL_LSCH6_S);
            break;

        case 7:
            SET_PERI_REG_BITS(LEDC_LSCH7_CONF0, LEDC_TIMER_SEL_LSCH7, timer_sel, LEDC_TIMER_SEL_LSCH7_S);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF0, LEDC_TIMER_SEL_LSCH0, timer_sel, LEDC_TIMER_SEL_LSCH0_S);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_set_htimer
 * Description  : high_speed channel x choose timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
                                  uint8 chan_num: 8 channels in total,value from
0~7
 * Returns      : NONE
*******************************************************************************/
void ledc_set_htimer(uint8 chan_num, uint8 timer_sel)
{
    switch (chan_num)
    {  //
        case 0:
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF0, LEDC_TIMER_SEL_HSCH0, timer_sel, LEDC_TIMER_SEL_HSCH0_S);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_HSCH1_CONF0, LEDC_TIMER_SEL_HSCH1, timer_sel, LEDC_TIMER_SEL_HSCH1_S);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_HSCH2_CONF0, LEDC_TIMER_SEL_HSCH2, timer_sel, LEDC_TIMER_SEL_HSCH2_S);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_HSCH3_CONF0, LEDC_TIMER_SEL_HSCH3, timer_sel, LEDC_TIMER_SEL_HSCH3_S);
            break;

        case 4:
            SET_PERI_REG_BITS(LEDC_HSCH4_CONF0, LEDC_TIMER_SEL_HSCH4, timer_sel, LEDC_TIMER_SEL_HSCH4_S);
            break;

        case 5:
            SET_PERI_REG_BITS(LEDC_HSCH5_CONF0, LEDC_TIMER_SEL_HSCH5, timer_sel, LEDC_TIMER_SEL_HSCH5_S);
            break;

        case 6:
            SET_PERI_REG_BITS(LEDC_HSCH6_CONF0, LEDC_TIMER_SEL_HSCH6, timer_sel, LEDC_TIMER_SEL_HSCH6_S);
            break;

        case 7:
            SET_PERI_REG_BITS(LEDC_HSCH7_CONF0, LEDC_TIMER_SEL_HSCH7, timer_sel, LEDC_TIMER_SEL_HSCH7_S);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF0, LEDC_TIMER_SEL_HSCH0, timer_sel, LEDC_TIMER_SEL_HSCH0_S);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_set_idle_hlevel
 * Description  : set high speed channel output (as high or low) when idle
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
                                  uint8 idle_level: 1-output high, 0-output low
 * Returns      : NONE
*******************************************************************************/
void ledc_set_idle_hlevel(uint8 chan_num, uint8 idle_level)
{
    if (idle_level)
    {
        switch (chan_num)
        {  //
            case 0:
                SET_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;

            case 1:
                SET_PERI_REG_MASK(LEDC_HSCH1_CONF0, LEDC_IDLE_LV_HSCH1);
                break;

            case 2:
                SET_PERI_REG_MASK(LEDC_HSCH2_CONF0, LEDC_IDLE_LV_HSCH2);
                break;

            case 3:
                SET_PERI_REG_MASK(LEDC_HSCH3_CONF0, LEDC_IDLE_LV_HSCH3);
                break;

            case 4:
                SET_PERI_REG_MASK(LEDC_HSCH4_CONF0, LEDC_IDLE_LV_HSCH4);
                break;

            case 5:
                SET_PERI_REG_MASK(LEDC_HSCH5_CONF0, LEDC_IDLE_LV_HSCH5);
                break;

            case 6:
                SET_PERI_REG_MASK(LEDC_HSCH6_CONF0, LEDC_IDLE_LV_HSCH6);
                break;

            case 7:
                SET_PERI_REG_MASK(LEDC_HSCH7_CONF0, LEDC_IDLE_LV_HSCH7);
                break;

            default:
                SET_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;
        }
    }
    else
    {
        switch (chan_num)
        {  //
            case 0:
                CLEAR_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;

            case 1:
                CLEAR_PERI_REG_MASK(LEDC_HSCH1_CONF0, LEDC_IDLE_LV_HSCH1);
                break;

            case 2:
                CLEAR_PERI_REG_MASK(LEDC_HSCH2_CONF0, LEDC_IDLE_LV_HSCH2);
                break;

            case 3:
                CLEAR_PERI_REG_MASK(LEDC_HSCH3_CONF0, LEDC_IDLE_LV_HSCH3);
                break;

            case 4:
                CLEAR_PERI_REG_MASK(LEDC_HSCH4_CONF0, LEDC_IDLE_LV_HSCH4);
                break;

            case 5:
                CLEAR_PERI_REG_MASK(LEDC_HSCH5_CONF0, LEDC_IDLE_LV_HSCH5);
                break;

            case 6:
                CLEAR_PERI_REG_MASK(LEDC_HSCH6_CONF0, LEDC_IDLE_LV_HSCH6);
                break;

            case 7:
                CLEAR_PERI_REG_MASK(LEDC_HSCH7_CONF0, LEDC_IDLE_LV_HSCH7);
                break;

            default:
                CLEAR_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : ledc_set_idle_llevel
 * Description  : set high speed channel output (as high or low) when idle
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
                                  uint8 idle_level: 1-output high, 0-output low
 * Returns      : NONE
*******************************************************************************/
void ledc_set_idle_llevel(uint8 chan_num, uint8 idle_level)
{
    if (idle_level)
    {
        switch (chan_num)
        {  //
            case 0:
                SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;

            case 1:
                SET_PERI_REG_MASK(LEDC_LSCH1_CONF0, LEDC_IDLE_LV_HSCH1);
                break;

            case 2:
                SET_PERI_REG_MASK(LEDC_LSCH2_CONF0, LEDC_IDLE_LV_HSCH2);
                break;

            case 3:
                SET_PERI_REG_MASK(LEDC_LSCH3_CONF0, LEDC_IDLE_LV_HSCH3);
                break;

            case 4:
                SET_PERI_REG_MASK(LEDC_LSCH4_CONF0, LEDC_IDLE_LV_HSCH4);
                break;

            case 5:
                SET_PERI_REG_MASK(LEDC_LSCH5_CONF0, LEDC_IDLE_LV_HSCH5);
                break;

            case 6:
                SET_PERI_REG_MASK(LEDC_LSCH6_CONF0, LEDC_IDLE_LV_HSCH6);
                break;

            case 7:
                SET_PERI_REG_MASK(LEDC_LSCH7_CONF0, LEDC_IDLE_LV_HSCH7);
                break;

            default:
                SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;
        }
    }
    else
    {
        switch (chan_num)
        {  //
            case 0:
                CLEAR_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;

            case 1:
                CLEAR_PERI_REG_MASK(LEDC_LSCH1_CONF0, LEDC_IDLE_LV_HSCH1);
                break;

            case 2:
                CLEAR_PERI_REG_MASK(LEDC_LSCH2_CONF0, LEDC_IDLE_LV_HSCH2);
                break;

            case 3:
                CLEAR_PERI_REG_MASK(LEDC_LSCH3_CONF0, LEDC_IDLE_LV_HSCH3);
                break;

            case 4:
                CLEAR_PERI_REG_MASK(LEDC_LSCH4_CONF0, LEDC_IDLE_LV_HSCH4);
                break;

            case 5:
                CLEAR_PERI_REG_MASK(LEDC_LSCH5_CONF0, LEDC_IDLE_LV_HSCH5);
                break;

            case 6:
                CLEAR_PERI_REG_MASK(LEDC_LSCH6_CONF0, LEDC_IDLE_LV_HSCH6);
                break;

            case 7:
                CLEAR_PERI_REG_MASK(LEDC_LSCH7_CONF0, LEDC_IDLE_LV_HSCH7);
                break;

            default:
                CLEAR_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_IDLE_LV_HSCH0);
                break;
        }
    }
}

/******************************************************************************
 * FunctionName : ledc_set_hduty
 * Description  : set high_speed channel duty
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
                                  uint32 hpoint_val:output high when counter
equals this value uint32 duty_val: output low after counter equals this value
                                  uint8 increase: 1-increase duty
ratio,0-decrease duty ratio uint16 duty_num: generate interrupt after duty_num *
duty_cycle outputs uint16 duty_cycle: increase or decrease duty ratio every
duty_cycle outputs uint16 duty_scale: the range of changing on duty ratio
 * Returns      : NONE
*******************************************************************************/
void ledc_set_hduty(uint8 chan_num, uint32 hpoint_val, uint32 duty_val, uint8 increase, uint16 duty_num,
                    uint16 duty_cycle, uint16 duty_scale)
{
    switch (chan_num)
    {
        case 0:
            SET_PERI_REG_BITS(LEDC_HSCH0_HPOINT, LEDC_HPOINT_HSCH0, hpoint_val, LEDC_HPOINT_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_DUTY, LEDC_DUTY_HSCH0, duty_val, LEDC_DUTY_HSCH0_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_INC_HSCH0);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_INC_HSCH0);
            }

            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_NUM_HSCH0, duty_num, LEDC_DUTY_NUM_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_CYCLE_HSCH0, duty_cycle, LEDC_DUTY_CYCLE_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_SCALE_HSCH0, duty_scale, LEDC_DUTY_SCALE_HSCH0_S);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_HSCH1_HPOINT, LEDC_HPOINT_HSCH1, hpoint_val, LEDC_HPOINT_HSCH1_S);
            SET_PERI_REG_BITS(LEDC_HSCH1_DUTY, LEDC_DUTY_HSCH1, duty_val, LEDC_DUTY_HSCH1_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH1_CONF1, LEDC_DUTY_INC_HSCH1);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH1_CONF1, LEDC_DUTY_INC_HSCH1);
            }

            SET_PERI_REG_BITS(LEDC_HSCH1_CONF1, LEDC_DUTY_NUM_HSCH1, duty_num, LEDC_DUTY_NUM_HSCH1_S);
            SET_PERI_REG_BITS(LEDC_HSCH1_CONF1, LEDC_DUTY_CYCLE_HSCH1, duty_cycle, LEDC_DUTY_CYCLE_HSCH1_S);
            SET_PERI_REG_BITS(LEDC_HSCH1_CONF1, LEDC_DUTY_SCALE_HSCH1, duty_scale, LEDC_DUTY_SCALE_HSCH1_S);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_HSCH2_HPOINT, LEDC_HPOINT_HSCH2, hpoint_val, LEDC_HPOINT_HSCH2_S);
            SET_PERI_REG_BITS(LEDC_HSCH2_DUTY, LEDC_DUTY_HSCH2, duty_val, LEDC_DUTY_HSCH2_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH2_CONF1, LEDC_DUTY_INC_HSCH2);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH2_CONF1, LEDC_DUTY_INC_HSCH2);
            }

            SET_PERI_REG_BITS(LEDC_HSCH2_CONF1, LEDC_DUTY_NUM_HSCH2, duty_num, LEDC_DUTY_NUM_HSCH2_S);
            SET_PERI_REG_BITS(LEDC_HSCH2_CONF1, LEDC_DUTY_CYCLE_HSCH2, duty_cycle, LEDC_DUTY_CYCLE_HSCH2_S);
            SET_PERI_REG_BITS(LEDC_HSCH2_CONF1, LEDC_DUTY_SCALE_HSCH2, duty_scale, LEDC_DUTY_SCALE_HSCH2_S);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_HSCH3_HPOINT, LEDC_HPOINT_HSCH3, hpoint_val, LEDC_HPOINT_HSCH3_S);
            SET_PERI_REG_BITS(LEDC_HSCH3_DUTY, LEDC_DUTY_HSCH3, duty_val, LEDC_DUTY_HSCH3_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH3_CONF1, LEDC_DUTY_INC_HSCH3);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH3_CONF1, LEDC_DUTY_INC_HSCH3);
            }

            SET_PERI_REG_BITS(LEDC_HSCH3_CONF1, LEDC_DUTY_NUM_HSCH3, duty_num, LEDC_DUTY_NUM_HSCH3_S);
            SET_PERI_REG_BITS(LEDC_HSCH3_CONF1, LEDC_DUTY_CYCLE_HSCH3, duty_cycle, LEDC_DUTY_CYCLE_HSCH3_S);
            SET_PERI_REG_BITS(LEDC_HSCH3_CONF1, LEDC_DUTY_SCALE_HSCH3, duty_scale, LEDC_DUTY_SCALE_HSCH3_S);
            break;

        case 4:
            SET_PERI_REG_BITS(LEDC_HSCH4_HPOINT, LEDC_HPOINT_HSCH4, hpoint_val, LEDC_HPOINT_HSCH4_S);
            SET_PERI_REG_BITS(LEDC_HSCH4_DUTY, LEDC_DUTY_HSCH4, duty_val, LEDC_DUTY_HSCH4_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH4_CONF1, LEDC_DUTY_INC_HSCH4);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH4_CONF1, LEDC_DUTY_INC_HSCH4);
            }

            SET_PERI_REG_BITS(LEDC_HSCH4_CONF1, LEDC_DUTY_NUM_HSCH4, duty_num, LEDC_DUTY_NUM_HSCH4_S);
            SET_PERI_REG_BITS(LEDC_HSCH4_CONF1, LEDC_DUTY_CYCLE_HSCH4, duty_cycle, LEDC_DUTY_CYCLE_HSCH4_S);
            SET_PERI_REG_BITS(LEDC_HSCH4_CONF1, LEDC_DUTY_SCALE_HSCH4, duty_scale, LEDC_DUTY_SCALE_HSCH4_S);
            break;

        case 5:
            SET_PERI_REG_BITS(LEDC_HSCH5_HPOINT, LEDC_HPOINT_HSCH5, hpoint_val, LEDC_HPOINT_HSCH5_S);
            SET_PERI_REG_BITS(LEDC_HSCH5_DUTY, LEDC_DUTY_HSCH5, duty_val, LEDC_DUTY_HSCH5_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH5_CONF1, LEDC_DUTY_INC_HSCH5);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH5_CONF1, LEDC_DUTY_INC_HSCH5);
            }

            SET_PERI_REG_BITS(LEDC_HSCH5_CONF1, LEDC_DUTY_NUM_HSCH5, duty_num, LEDC_DUTY_NUM_HSCH5_S);
            SET_PERI_REG_BITS(LEDC_HSCH5_CONF1, LEDC_DUTY_CYCLE_HSCH5, duty_cycle, LEDC_DUTY_CYCLE_HSCH5_S);
            SET_PERI_REG_BITS(LEDC_HSCH5_CONF1, LEDC_DUTY_SCALE_HSCH5, duty_scale, LEDC_DUTY_SCALE_HSCH5_S);
            break;

        case 6:
            SET_PERI_REG_BITS(LEDC_HSCH6_HPOINT, LEDC_HPOINT_HSCH6, hpoint_val, LEDC_HPOINT_HSCH6_S);
            SET_PERI_REG_BITS(LEDC_HSCH6_DUTY, LEDC_DUTY_HSCH6, duty_val, LEDC_DUTY_HSCH6_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH6_CONF1, LEDC_DUTY_INC_HSCH6);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH6_CONF1, LEDC_DUTY_INC_HSCH6);
            }

            SET_PERI_REG_BITS(LEDC_HSCH6_CONF1, LEDC_DUTY_NUM_HSCH6, duty_num, LEDC_DUTY_NUM_HSCH6_S);
            SET_PERI_REG_BITS(LEDC_HSCH6_CONF1, LEDC_DUTY_CYCLE_HSCH6, duty_cycle, LEDC_DUTY_CYCLE_HSCH6_S);
            SET_PERI_REG_BITS(LEDC_HSCH6_CONF1, LEDC_DUTY_SCALE_HSCH6, duty_scale, LEDC_DUTY_SCALE_HSCH6_S);
            break;

        case 7:
            SET_PERI_REG_BITS(LEDC_HSCH7_HPOINT, LEDC_HPOINT_HSCH7, hpoint_val, LEDC_HPOINT_HSCH7_S);
            SET_PERI_REG_BITS(LEDC_HSCH7_DUTY, LEDC_DUTY_HSCH7, duty_val, LEDC_DUTY_HSCH7_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH7_CONF1, LEDC_DUTY_INC_HSCH7);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH7_CONF1, LEDC_DUTY_INC_HSCH7);
            }

            SET_PERI_REG_BITS(LEDC_HSCH7_CONF1, LEDC_DUTY_NUM_HSCH7, duty_num, LEDC_DUTY_NUM_HSCH7_S);
            SET_PERI_REG_BITS(LEDC_HSCH7_CONF1, LEDC_DUTY_CYCLE_HSCH7, duty_cycle, LEDC_DUTY_CYCLE_HSCH7_S);
            SET_PERI_REG_BITS(LEDC_HSCH7_CONF1, LEDC_DUTY_SCALE_HSCH7, duty_scale, LEDC_DUTY_SCALE_HSCH7_S);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_HSCH0_HPOINT, LEDC_HPOINT_HSCH0, hpoint_val, LEDC_HPOINT_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_DUTY, LEDC_DUTY_HSCH0, duty_val, LEDC_DUTY_HSCH0_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_INC_HSCH0);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_INC_HSCH0);
            }

            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_NUM_HSCH0, duty_num, LEDC_DUTY_NUM_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_CYCLE_HSCH0, duty_cycle, LEDC_DUTY_CYCLE_HSCH0_S);
            SET_PERI_REG_BITS(LEDC_HSCH0_CONF1, LEDC_DUTY_SCALE_HSCH0, duty_scale, LEDC_DUTY_SCALE_HSCH0_S);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_set_lduty
 * Description  : set low_speed channel duty
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
                                  uint32 hpoint_val:output high when counter
equals this value uint32 duty_val: output low after counter equals this value
                                  uint8 increase: 1-increase duty
ratio,0-decrease duty ratio uint16 duty_num: generate interrupt after duty_num *
duty_cycle outputs uint16 duty_cycle: increase or decrease duty ratio every
duty_cycle outputs uint16 duty_scale: the range of changing on duty ratio
 * Returns      : NONE
*******************************************************************************/
void ledc_set_lduty(uint8 chan_num, uint32 hpoint_val, uint32 duty_val, uint8 increase, uint16 duty_num,
                    uint16 duty_cycle, uint16 duty_scale)
{
    switch (chan_num)
    {
        case 0:
            SET_PERI_REG_BITS(LEDC_LSCH0_HPOINT, LEDC_HPOINT_LSCH0, hpoint_val, LEDC_HPOINT_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_DUTY, LEDC_DUTY_LSCH0, duty_val, LEDC_DUTY_LSCH0_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_INC_LSCH0);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_INC_LSCH0);
            }

            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_NUM_LSCH0, duty_num, LEDC_DUTY_NUM_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_CYCLE_LSCH0, duty_cycle, LEDC_DUTY_CYCLE_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_SCALE_LSCH0, duty_scale, LEDC_DUTY_SCALE_LSCH0_S);
            break;

        case 1:
            SET_PERI_REG_BITS(LEDC_LSCH1_HPOINT, LEDC_HPOINT_LSCH1, hpoint_val, LEDC_HPOINT_LSCH1_S);
            SET_PERI_REG_BITS(LEDC_LSCH1_DUTY, LEDC_DUTY_LSCH1, duty_val, LEDC_DUTY_LSCH1_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH1_CONF1, LEDC_DUTY_INC_LSCH1);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH1_CONF1, LEDC_DUTY_INC_LSCH1);
            }

            SET_PERI_REG_BITS(LEDC_LSCH1_CONF1, LEDC_DUTY_NUM_LSCH1, duty_num, LEDC_DUTY_NUM_LSCH1_S);
            SET_PERI_REG_BITS(LEDC_LSCH1_CONF1, LEDC_DUTY_CYCLE_LSCH1, duty_cycle, LEDC_DUTY_CYCLE_LSCH1_S);
            SET_PERI_REG_BITS(LEDC_LSCH1_CONF1, LEDC_DUTY_SCALE_LSCH1, duty_scale, LEDC_DUTY_SCALE_LSCH1_S);
            break;

        case 2:
            SET_PERI_REG_BITS(LEDC_LSCH2_HPOINT, LEDC_HPOINT_LSCH2, hpoint_val, LEDC_HPOINT_LSCH2_S);
            SET_PERI_REG_BITS(LEDC_LSCH2_DUTY, LEDC_DUTY_LSCH2, duty_val, LEDC_DUTY_LSCH2_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH2_CONF1, LEDC_DUTY_INC_LSCH2);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH2_CONF1, LEDC_DUTY_INC_LSCH2);
            }

            SET_PERI_REG_BITS(LEDC_LSCH2_CONF1, LEDC_DUTY_NUM_LSCH2, duty_num, LEDC_DUTY_NUM_LSCH2_S);
            SET_PERI_REG_BITS(LEDC_LSCH2_CONF1, LEDC_DUTY_CYCLE_LSCH2, duty_cycle, LEDC_DUTY_CYCLE_LSCH2_S);
            SET_PERI_REG_BITS(LEDC_LSCH2_CONF1, LEDC_DUTY_SCALE_LSCH2, duty_scale, LEDC_DUTY_SCALE_LSCH2_S);
            break;

        case 3:
            SET_PERI_REG_BITS(LEDC_LSCH3_HPOINT, LEDC_HPOINT_LSCH3, hpoint_val, LEDC_HPOINT_LSCH3_S);
            SET_PERI_REG_BITS(LEDC_LSCH3_DUTY, LEDC_DUTY_LSCH3, duty_val, LEDC_DUTY_LSCH3_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH3_CONF1, LEDC_DUTY_INC_LSCH3);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH3_CONF1, LEDC_DUTY_INC_LSCH3);
            }

            SET_PERI_REG_BITS(LEDC_LSCH3_CONF1, LEDC_DUTY_NUM_LSCH3, duty_num, LEDC_DUTY_NUM_LSCH3_S);
            SET_PERI_REG_BITS(LEDC_LSCH3_CONF1, LEDC_DUTY_CYCLE_LSCH3, duty_cycle, LEDC_DUTY_CYCLE_LSCH3_S);
            SET_PERI_REG_BITS(LEDC_LSCH3_CONF1, LEDC_DUTY_SCALE_LSCH3, duty_scale, LEDC_DUTY_SCALE_LSCH3_S);
            break;

        case 4:
            SET_PERI_REG_BITS(LEDC_LSCH4_HPOINT, LEDC_HPOINT_LSCH4, hpoint_val, LEDC_HPOINT_LSCH4_S);
            SET_PERI_REG_BITS(LEDC_LSCH4_DUTY, LEDC_DUTY_LSCH4, duty_val, LEDC_DUTY_LSCH4_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH4_CONF1, LEDC_DUTY_INC_LSCH4);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH4_CONF1, LEDC_DUTY_INC_LSCH4);
            }

            SET_PERI_REG_BITS(LEDC_LSCH4_CONF1, LEDC_DUTY_NUM_LSCH4, duty_num, LEDC_DUTY_NUM_LSCH4_S);
            SET_PERI_REG_BITS(LEDC_LSCH4_CONF1, LEDC_DUTY_CYCLE_LSCH4, duty_cycle, LEDC_DUTY_CYCLE_LSCH4_S);
            SET_PERI_REG_BITS(LEDC_LSCH4_CONF1, LEDC_DUTY_SCALE_LSCH4, duty_scale, LEDC_DUTY_SCALE_LSCH4_S);
            break;

        case 5:
            SET_PERI_REG_BITS(LEDC_LSCH5_HPOINT, LEDC_HPOINT_LSCH5, hpoint_val, LEDC_HPOINT_LSCH5_S);
            SET_PERI_REG_BITS(LEDC_LSCH5_DUTY, LEDC_DUTY_LSCH5, duty_val, LEDC_DUTY_LSCH5_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH5_CONF1, LEDC_DUTY_INC_LSCH5);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH5_CONF1, LEDC_DUTY_INC_LSCH5);
            }

            SET_PERI_REG_BITS(LEDC_LSCH5_CONF1, LEDC_DUTY_NUM_LSCH5, duty_num, LEDC_DUTY_NUM_LSCH5_S);
            SET_PERI_REG_BITS(LEDC_LSCH5_CONF1, LEDC_DUTY_CYCLE_LSCH5, duty_cycle, LEDC_DUTY_CYCLE_LSCH5_S);
            SET_PERI_REG_BITS(LEDC_LSCH5_CONF1, LEDC_DUTY_SCALE_LSCH5, duty_scale, LEDC_DUTY_SCALE_LSCH5_S);
            break;

        case 6:
            SET_PERI_REG_BITS(LEDC_LSCH6_HPOINT, LEDC_HPOINT_LSCH6, hpoint_val, LEDC_HPOINT_LSCH6_S);
            SET_PERI_REG_BITS(LEDC_LSCH6_DUTY, LEDC_DUTY_LSCH6, duty_val, LEDC_DUTY_LSCH6_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH6_CONF1, LEDC_DUTY_INC_LSCH6);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH6_CONF1, LEDC_DUTY_INC_LSCH6);
            }

            SET_PERI_REG_BITS(LEDC_LSCH6_CONF1, LEDC_DUTY_NUM_LSCH6, duty_num, LEDC_DUTY_NUM_LSCH6_S);
            SET_PERI_REG_BITS(LEDC_LSCH6_CONF1, LEDC_DUTY_CYCLE_LSCH6, duty_cycle, LEDC_DUTY_CYCLE_LSCH6_S);
            SET_PERI_REG_BITS(LEDC_LSCH6_CONF1, LEDC_DUTY_SCALE_LSCH6, duty_scale, LEDC_DUTY_SCALE_LSCH6_S);
            break;

        case 7:
            SET_PERI_REG_BITS(LEDC_LSCH7_HPOINT, LEDC_HPOINT_LSCH7, hpoint_val, LEDC_HPOINT_LSCH7_S);
            SET_PERI_REG_BITS(LEDC_LSCH7_DUTY, LEDC_DUTY_LSCH7, duty_val, LEDC_DUTY_LSCH7_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH7_CONF1, LEDC_DUTY_INC_LSCH7);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH7_CONF1, LEDC_DUTY_INC_LSCH7);
            }

            SET_PERI_REG_BITS(LEDC_LSCH7_CONF1, LEDC_DUTY_NUM_LSCH7, duty_num, LEDC_DUTY_NUM_LSCH7_S);
            SET_PERI_REG_BITS(LEDC_LSCH7_CONF1, LEDC_DUTY_CYCLE_LSCH7, duty_cycle, LEDC_DUTY_CYCLE_LSCH7_S);
            SET_PERI_REG_BITS(LEDC_LSCH7_CONF1, LEDC_DUTY_SCALE_LSCH7, duty_scale, LEDC_DUTY_SCALE_LSCH7_S);
            break;

        default:
            SET_PERI_REG_BITS(LEDC_LSCH0_HPOINT, LEDC_HPOINT_LSCH0, hpoint_val, LEDC_HPOINT_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_DUTY, LEDC_DUTY_LSCH0, duty_val, LEDC_DUTY_LSCH0_S);

            if (increase == 1)
            {
                SET_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_INC_LSCH0);
            }
            else
            {
                CLEAR_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_INC_LSCH0);
            }

            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_NUM_LSCH0, duty_num, LEDC_DUTY_NUM_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_CYCLE_LSCH0, duty_cycle, LEDC_DUTY_CYCLE_LSCH0_S);
            SET_PERI_REG_BITS(LEDC_LSCH0_CONF1, LEDC_DUTY_SCALE_LSCH0, duty_scale, LEDC_DUTY_SCALE_LSCH0_S);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_hstart
 * Description  : enable one high_speed channel
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
 * Returns      : NONE
 *******************************************************************************/
void ledc_hstart(uint8 chan_num)
{
    switch (chan_num)
    {  //
        case 0:
            SET_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_START_HSCH0);
            SET_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_SIG_OUT_EN_HSCH0);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_HSCH1_CONF1, LEDC_DUTY_START_HSCH1);
            SET_PERI_REG_MASK(LEDC_HSCH1_CONF0, LEDC_SIG_OUT_EN_HSCH1);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_HSCH2_CONF1, LEDC_DUTY_START_HSCH2);
            SET_PERI_REG_MASK(LEDC_HSCH2_CONF0, LEDC_SIG_OUT_EN_HSCH2);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_HSCH3_CONF1, LEDC_DUTY_START_HSCH3);
            SET_PERI_REG_MASK(LEDC_HSCH3_CONF0, LEDC_SIG_OUT_EN_HSCH3);
            break;

        case 4:
            SET_PERI_REG_MASK(LEDC_HSCH4_CONF1, LEDC_DUTY_START_HSCH4);
            SET_PERI_REG_MASK(LEDC_HSCH4_CONF0, LEDC_SIG_OUT_EN_HSCH4);
            break;

        case 5:
            SET_PERI_REG_MASK(LEDC_HSCH5_CONF1, LEDC_DUTY_START_HSCH5);
            SET_PERI_REG_MASK(LEDC_HSCH5_CONF0, LEDC_SIG_OUT_EN_HSCH5);
            break;

        case 6:
            SET_PERI_REG_MASK(LEDC_HSCH6_CONF1, LEDC_DUTY_START_HSCH6);
            SET_PERI_REG_MASK(LEDC_HSCH6_CONF0, LEDC_SIG_OUT_EN_HSCH6);
            break;

        case 7:
            SET_PERI_REG_MASK(LEDC_HSCH7_CONF1, LEDC_DUTY_START_HSCH7);
            SET_PERI_REG_MASK(LEDC_HSCH7_CONF0, LEDC_SIG_OUT_EN_HSCH7);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_HSCH0_CONF1, LEDC_DUTY_START_HSCH0);
            SET_PERI_REG_MASK(LEDC_HSCH0_CONF0, LEDC_SIG_OUT_EN_HSCH0);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_lstart
 * Description  : enable one low_speed channel
 * Parameters   : uint8 chan_num: 8 channels in total,value from 0~7
 * Returns      : NONE
 *******************************************************************************/
void ledc_lstart(uint8 chan_num)
{
    switch (chan_num)
    {  //
        case 0:
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_PARA_UP_LSCH0);
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_START_LSCH0);
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_SIG_OUT_EN_LSCH0);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_LSCH1_CONF0, LEDC_PARA_UP_LSCH1);
            SET_PERI_REG_MASK(LEDC_LSCH1_CONF1, LEDC_DUTY_START_LSCH1);
            SET_PERI_REG_MASK(LEDC_LSCH1_CONF0, LEDC_SIG_OUT_EN_LSCH1);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_LSCH2_CONF0, LEDC_PARA_UP_LSCH2);
            SET_PERI_REG_MASK(LEDC_LSCH2_CONF1, LEDC_DUTY_START_LSCH2);
            SET_PERI_REG_MASK(LEDC_LSCH2_CONF0, LEDC_SIG_OUT_EN_LSCH2);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_LSCH3_CONF0, LEDC_PARA_UP_LSCH3);
            SET_PERI_REG_MASK(LEDC_LSCH3_CONF1, LEDC_DUTY_START_LSCH3);
            SET_PERI_REG_MASK(LEDC_LSCH3_CONF0, LEDC_SIG_OUT_EN_LSCH3);
            break;

        case 4:
            SET_PERI_REG_MASK(LEDC_LSCH4_CONF0, LEDC_PARA_UP_LSCH4);
            SET_PERI_REG_MASK(LEDC_LSCH4_CONF1, LEDC_DUTY_START_LSCH4);
            SET_PERI_REG_MASK(LEDC_LSCH4_CONF0, LEDC_SIG_OUT_EN_LSCH4);
            break;

        case 5:
            SET_PERI_REG_MASK(LEDC_LSCH5_CONF0, LEDC_PARA_UP_LSCH5);
            SET_PERI_REG_MASK(LEDC_LSCH5_CONF1, LEDC_DUTY_START_LSCH5);
            SET_PERI_REG_MASK(LEDC_LSCH5_CONF0, LEDC_SIG_OUT_EN_LSCH5);
            break;

        case 6:
            SET_PERI_REG_MASK(LEDC_LSCH6_CONF0, LEDC_PARA_UP_LSCH6);
            SET_PERI_REG_MASK(LEDC_LSCH6_CONF1, LEDC_DUTY_START_LSCH6);
            SET_PERI_REG_MASK(LEDC_LSCH6_CONF0, LEDC_SIG_OUT_EN_LSCH6);
            break;

        case 7:
            SET_PERI_REG_MASK(LEDC_LSCH7_CONF0, LEDC_PARA_UP_LSCH7);
            SET_PERI_REG_MASK(LEDC_LSCH7_CONF1, LEDC_DUTY_START_LSCH7);
            SET_PERI_REG_MASK(LEDC_LSCH7_CONF0, LEDC_SIG_OUT_EN_LSCH7);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_PARA_UP_LSCH0);
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF1, LEDC_DUTY_START_LSCH0);
            SET_PERI_REG_MASK(LEDC_LSCH0_CONF0, LEDC_SIG_OUT_EN_LSCH0);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_hpause
 * Description  : pause the select high_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_hpause(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_PAUSE);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_HSTIMER1_CONF, LEDC_HSTIMER1_PAUSE);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_HSTIMER2_CONF, LEDC_HSTIMER2_PAUSE);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_HSTIMER3_CONF, LEDC_HSTIMER3_PAUSE);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_PAUSE);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_lpause
 * Description  : pause the select low_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_lpause(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PAUSE);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_LSTIMER1_CONF, LEDC_LSTIMER1_PAUSE);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_LSTIMER2_CONF, LEDC_LSTIMER2_PAUSE);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_LSTIMER3_CONF, LEDC_LSTIMER3_PAUSE);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PAUSE);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_hunpause
 * Description  : unpause the select high_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_hunpause(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            CLEAR_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_PAUSE);
            break;

        case 1:
            CLEAR_PERI_REG_MASK(LEDC_HSTIMER1_CONF, LEDC_HSTIMER1_PAUSE);
            break;

        case 2:
            CLEAR_PERI_REG_MASK(LEDC_HSTIMER2_CONF, LEDC_HSTIMER2_PAUSE);
            break;

        case 3:
            CLEAR_PERI_REG_MASK(LEDC_HSTIMER3_CONF, LEDC_HSTIMER3_PAUSE);
            break;

        default:
            CLEAR_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_PAUSE);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_lunpause
 * Description  : unpause the select low_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_lunpause(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            CLEAR_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PAUSE);
            break;

        case 1:
            CLEAR_PERI_REG_MASK(LEDC_LSTIMER1_CONF, LEDC_LSTIMER1_PAUSE);
            break;

        case 2:
            CLEAR_PERI_REG_MASK(LEDC_LSTIMER2_CONF, LEDC_LSTIMER2_PAUSE);
            break;

        case 3:
            CLEAR_PERI_REG_MASK(LEDC_LSTIMER3_CONF, LEDC_LSTIMER3_PAUSE);
            break;

        default:
            CLEAR_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_PAUSE);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_hstop
 * Description  : disable the select high_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_hstop(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_RST);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_HSTIMER1_CONF, LEDC_HSTIMER1_RST);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_HSTIMER2_CONF, LEDC_HSTIMER2_RST);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_HSTIMER3_CONF, LEDC_HSTIMER3_RST);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_HSTIMER0_CONF, LEDC_HSTIMER0_RST);
            break;
    }
}

/******************************************************************************
 * FunctionName : ledc_timer_lstop
 * Description  : disable the select low_speed timer
 * Parameters   : uint8 timer_sel: 0-timer0, 1-timer1, 2-timer2, 3-timer3
 * Returns      : NONE
 *******************************************************************************/
void ledc_timer_lstop(uint8 timer_sel)
{
    switch (timer_sel)
    {
        case 0:
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_RST);
            break;

        case 1:
            SET_PERI_REG_MASK(LEDC_LSTIMER1_CONF, LEDC_LSTIMER1_RST);
            break;

        case 2:
            SET_PERI_REG_MASK(LEDC_LSTIMER2_CONF, LEDC_LSTIMER2_RST);
            break;

        case 3:
            SET_PERI_REG_MASK(LEDC_LSTIMER3_CONF, LEDC_LSTIMER3_RST);
            break;

        default:
            SET_PERI_REG_MASK(LEDC_LSTIMER0_CONF, LEDC_LSTIMER0_RST);
            break;
    }
}

void pwm_start(void)
{
    ledc_hstart(0);
    ledc_hstart(1);
    ledc_hstart(2);
    ledc_hstart(3);
    ledc_hstart(4);
    ledc_hstart(5);
    ledc_hstart(6);
    ledc_hstart(7);
}

/*set high speed channel duty*/
void pwm_set_duty(uint32 duty, uint8 channel)
{
    uint8 chan_num;

    for (chan_num = 0; chan_num < channel; chan_num++)
    {
        ledc_set_hduty(chan_num,  // uint8 chan_num,
                       0,         // uint32 hpoint_val,
                       duty,      // uint32 duty_val,
                       1,         // uint8 increase,
                       1,         // uint16 duty_num,
                       1,         // uint16 duty_cycle,
                       0          // uint16 duty_scale
        );
    }
}

uint32 pwm_get_duty(uint8 channel)
{
    uint32 duty_val;
    duty_val = READ_PERI_REG(LEDC_HSCH6_DUTY);
    return duty_val;
}

/*period = Freq(base_clk)/512/period*256 */
void pwm_set_period(uint32 period) { ledc_set_hperiod(0, 512, period); }

/*calculate frequency div_num*/
uint32 pwm_get_period(void)
{
    uint32 div_lv1, div_lv2, div_freq;
    uint32 timer0_info;
    timer0_info = READ_PERI_REG(LEDC_HSTIMER0_CONF);
    div_lv1 = timer0_info & LEDC_HSTIMER0_LIM;
    div_lv2 = (timer0_info >> LEDC_DIV_NUM_HSTIMER0_S) & LEDC_DIV_NUM_HSTIMER0;
    div_freq = ((div_lv1 * div_lv2) >> 8);
    return div_freq;
}

void pwm_init(uint32 period, uint32 *duty, uint32 pwm_channel_num, uint32 (*pin_info_list)[3])
{
    pwm_set_period(period);
    pwm_set_duty(*duty, pwm_channel_num);
    pwm_start();
}

bool pwm_exist(uint8 channel)
{
    // PWM_DBG("--Function pwm_exist() is called. channel:%d\n", channel);
    // PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    // PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    // PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i;
    for (i = 0; i < PWM_CHANNEL_NUM_MAX; i++)
    {
        if (pwm_out_io_num[i] == channel)  // exist
            return true;
    }
    return false;
}

uint16 pwm_get_freq(uint8 channel) { return pwm.freq; }

LOCAL volatile uint8 critical = 0;

#define LOCK_PWM(c)      \
    do                   \
    {                    \
        while ((c) == 1) \
            ;            \
        (c) = 1;         \
    } while (0)

#define UNLOCK_PWM(c) \
    do                \
    {                 \
        (c) = 0;      \
    } while (0)

void pwm_set_freq(uint16 freq, uint8 channel)
{
    LOCK_PWM(critical);  // enter critical
    if (freq > PWM_FREQ_MAX)
    {
        pwm.freq = PWM_FREQ_MAX;
    }
    else if (freq < 1)
    {
        pwm.freq = 1;
    }
    else
    {
        pwm.freq = freq;
    }

    pwm.period = PWM_1S / pwm.freq;
    UNLOCK_PWM(critical);  // leave critical
}

bool pwm_add(uint8 channel)
{
    // PWM_DBG("--Function pwm_add() is called. channel:%d\n", channel);
    // PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    // PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    // PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i;
    for (i = 0; i < PWM_CHANNEL_NUM_MAX; i++)
    {
        if (pwm_out_io_num[i] == channel)  // already exist
            return true;
        if (pwm_out_io_num[i] == -1)
        {                        // empty exist
            LOCK_PWM(critical);  // enter critical
            pwm_out_io_num[i] = channel;
            pwm.duty[i] = 0;
            pwm_gpio |= (1 << pin_num[channel]);
            PIN_FUNC_SELECT(pin_mux[channel], pin_func[channel]);
            GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel])),
                           GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel]))) &
                               (~GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)));  // disable open drain;
            pwm_channel_num++;
            UNLOCK_PWM(critical);  // leave critical
            return true;
        }
    }
    return false;
}

bool pwm_delete(uint8 channel)
{
    // PWM_DBG("--Function pwm_delete() is called. channel:%d\n", channel);
    // PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    // PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    // PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i, j;
    for (i = 0; i < pwm_channel_num; i++)
    {
        if (pwm_out_io_num[i] == channel)
        {                        // exist
            LOCK_PWM(critical);  // enter critical
            pwm_out_io_num[i] = -1;
            pwm_gpio &= ~(1 << pin_num[channel]);  // clear the bit
            for (j = i; j < pwm_channel_num - 1; j++)
            {
                pwm_out_io_num[j] = pwm_out_io_num[j + 1];
                pwm.duty[j] = pwm.duty[j + 1];
            }
            pwm_out_io_num[pwm_channel_num - 1] = -1;
            pwm.duty[pwm_channel_num - 1] = 0;
            pwm_channel_num--;
            UNLOCK_PWM(critical);  // leave critical
            return true;
        }
    }
    // non found
    return true;
}
