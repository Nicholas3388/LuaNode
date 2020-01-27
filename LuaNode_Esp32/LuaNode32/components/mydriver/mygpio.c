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

#include "mygpio.h"

#include <stdio.h>

#include "esp_common.h"
#include "extras/gpio_ext.h"
#include "extras/gpio_reg_ext.h"
#include "extras/gpio_sd_reg_ext.h"
#include "freertos/portmacro.h"
#include "freertos/xtensa_api.h"
#include "rom/ets_sys.h"
#include "rom/gpio.h"
#include "soc/gpio_reg.h"

const uint32 GPIO_PIN_REG[40] = {GPIO_PIN_REG_0,
                                 GPIO_PIN_REG_1,
                                 GPIO_PIN_REG_2,
                                 GPIO_PIN_REG_3,
                                 GPIO_PIN_REG_4,
                                 GPIO_PIN_REG_5,
                                 GPIO_PIN_REG_6,
                                 GPIO_PIN_REG_7,
                                 GPIO_PIN_REG_8,
                                 GPIO_PIN_REG_9,
                                 GPIO_PIN_REG_10,
                                 GPIO_PIN_REG_11,
                                 GPIO_PIN_REG_12,
                                 GPIO_PIN_REG_13,
                                 GPIO_PIN_REG_14,
                                 GPIO_PIN_REG_15,
                                 GPIO_PIN_REG_16,
                                 GPIO_PIN_REG_17,
                                 GPIO_PIN_REG_18,
                                 GPIO_PIN_REG_19,
                                 GPIO_PIN_REG_20,
                                 GPIO_PIN_REG_21,
                                 GPIO_PIN_REG_22,
                                 GPIO_PIN_REG_23,
                                 0,
                                 GPIO_PIN_REG_25,
                                 GPIO_PIN_REG_26,
                                 GPIO_PIN_REG_27,
                                 0,
                                 0,
                                 0,
                                 0,
                                 GPIO_PIN_REG_32,
                                 GPIO_PIN_REG_33,
                                 GPIO_PIN_REG_34,
                                 GPIO_PIN_REG_35,
                                 GPIO_PIN_REG_36,
                                 GPIO_PIN_REG_37,
                                 GPIO_PIN_REG_38,
                                 GPIO_PIN_REG_39};

const uint8 intr_gpio_signals[] = {PCNT_SIG_CH0_IN0_IDX, PCNT_SIG_CH0_IN1_IDX, PCNT_SIG_CH0_IN2_IDX,
                                   PCNT_SIG_CH0_IN3_IDX, PCNT_SIG_CH0_IN4_IDX, PCNT_SIG_CH0_IN5_IDX,
                                   PCNT_SIG_CH0_IN6_IDX, PCNT_SIG_CH0_IN7_IDX};

uint8 intr_gpio_nums[8];

static portMUX_TYPE g_gpio_mux = portMUX_INITIALIZER_UNLOCKED;

static void gpio_intr_reset(uint32 intr_num, uint8 reset)
{
    if (intr_num > 7)
    {
        return;
    }

    // bit PCNT_CNT_PAUSE_U0-7
    CLEAR_PERI_REG_MASK(PCNT_CTRL, BIT(intr_num * 2 + 1));

    // bit PCNT_PLUS_CNT_RST_U0-7
    if (reset)
    {
        SET_PERI_REG_MASK(PCNT_CTRL, BIT(intr_num * 2));
    }
    else
    {
        CLEAR_PERI_REG_MASK(PCNT_CTRL, BIT(intr_num * 2));
    }
}

static void gpio_intr_init(uint32 intr_num, gpio_int_type_t intr_type)
{
    uint32 cfg0_addr = PCNT_U0_CONF0 + (intr_num * 12);
    uint32 cfg1_addr = PCNT_U0_CONF1 + (intr_num * 12);
    uint32 cfg2_addr = PCNT_U0_CONF2 + (intr_num * 12);

    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH1_LCTRL_MODE_U0, 0, PCNT_CH1_LCTRL_MODE_U0_S);
    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH1_HCTRL_MODE_U0, 0, PCNT_CH1_HCTRL_MODE_U0_S);
    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH1_POS_MODE_U0, 0, PCNT_CH1_POS_MODE_U0_S);
    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH1_NEG_MODE_U0, 0, PCNT_CH1_NEG_MODE_U0_S);
    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_LCTRL_MODE_U0, 0, PCNT_CH0_LCTRL_MODE_U0_S);
    SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_HCTRL_MODE_U0, 0, PCNT_CH0_HCTRL_MODE_U0_S);

    if (intr_type == GPIO_INTR_NEGEDGE)
    {
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_POS_MODE_U0, 0, PCNT_CH0_POS_MODE_U0_S);
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_NEG_MODE_U0, 1, PCNT_CH0_NEG_MODE_U0_S);
    }
    else if (intr_type == GPIO_INTR_POSEDGE)
    {
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_POS_MODE_U0, 1, PCNT_CH0_POS_MODE_U0_S);
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_NEG_MODE_U0, 0, PCNT_CH0_NEG_MODE_U0_S);
    }
    else if (intr_type == GPIO_INTR_ANYEDGE)
    {
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_POS_MODE_U0, 1, PCNT_CH0_POS_MODE_U0_S);
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_NEG_MODE_U0, 1, PCNT_CH0_NEG_MODE_U0_S);
    }
    else
    {
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_POS_MODE_U0, 0, PCNT_CH0_POS_MODE_U0_S);
        SET_PERI_REG_BITS(cfg0_addr, PCNT_CH0_NEG_MODE_U0, 0, PCNT_CH0_NEG_MODE_U0_S);
    }

    SET_PERI_REG_BITS(cfg1_addr, PCNT_CNT_THRES0_U0, 1, PCNT_CNT_THRES0_U0_S);
    SET_PERI_REG_BITS(cfg2_addr, PCNT_CNT_L_LIM_U0, 10, PCNT_CNT_L_LIM_U0_S);
    SET_PERI_REG_BITS(cfg2_addr, PCNT_CNT_H_LIM_U0, 10, PCNT_CNT_H_LIM_U0_S);
    CLEAR_PERI_REG_MASK(cfg0_addr, (PCNT_THR_THRES1_EN_U0 | PCNT_THR_L_LIM_EN_U0 | PCNT_THR_H_LIM_EN_U0 |
                                    PCNT_THR_ZERO_EN_U0 | PCNT_FILTER_EN_U0));

    SET_PERI_REG_MASK(cfg0_addr, PCNT_THR_THRES0_EN_U0);
    SET_PERI_REG_MASK(PCNT_INT_ENA, BIT(intr_num));
}

// intr_num only support 0-7
void gpio_intr_config(uint32 gpio_num, uint32 intr_num, GPIO_INT_TYPE intr_type)
{
    if (intr_num >= sizeof(intr_gpio_nums))
    {
        return;
    }

    gpio_intr_reset(intr_num, 1);
    intr_gpio_nums[intr_num] = gpio_num;
    gpio_matrix_in(0x30, intr_gpio_signals[intr_num], false);
    gpio_matrix_in(intr_gpio_nums[intr_num], intr_gpio_signals[intr_num], false);
    gpio_intr_init(intr_num, intr_type);
    gpio_intr_reset(intr_num, 0);
}

/*void gpio_intr_disable(uint32 intr_num)
{
    uint32 cfg0_addr = PCNT_U0_CONF0 + (intr_num * 12);
    gpio_intr_reset(intr_num, 1);
    CLEAR_PERI_REG_MASK(cfg0_addr, PCNT_THR_THRES0_EN_U0);
    CLEAR_PERI_REG_MASK(PCNT_INT_ENA, BIT(intr_num));
}*/

static void gpio_intr_clear(uint32 intr_num)
{
    SET_PERI_REG_MASK(PCNT_INT_CLR, BIT(intr_num));
    gpio_intr_reset(intr_num, 1);
    gpio_intr_reset(intr_num, 0);
}

void gpio_intr_process(void)
{
    uint32 intr_status;
    int intr_num;
    uint8 gpio_num;
    intr_status = READ_PERI_REG(PCNT_INT_ST);

    while ((intr_num = __builtin_ctz(intr_status)) >= 0)
    {
        intr_status &= ~BIT(intr_num);
        gpio_num = intr_gpio_nums[intr_num];
        gpio_intr_clear(intr_num);

        switch (gpio_num)
        {
            default:
                printf("gpio %d intr come\n", gpio_num);
                break;
        }
    }
}

/*void gpio_config(GPIO_ConfigTypeDef *pGPIOConfig)
{
    uint32 gpio_pin_mask = pGPIOConfig->GPIO_Pin;
    uint32 gpio_pin_mask_high = pGPIOConfig->GPIO_Pin_high;
    uint32 io_reg;
    uint8 io_num = 0;
    uint32 pin_reg;
    uint32 bit_valid;

    if (pGPIOConfig->GPIO_Mode == GPIO_Mode_Input) {
        GPIO_AS_INPUT(gpio_pin_mask);
        GPIO_AS_INPUT_HIGH(gpio_pin_mask_high);
    } else if (pGPIOConfig->GPIO_Mode == GPIO_Mode_Output) {
        GPIO_AS_OUTPUT(gpio_pin_mask);
        GPIO_AS_OUTPUT_HIGH(gpio_pin_mask_high);
    }

    do {
        bit_valid = (io_num >= 32 ? (gpio_pin_mask_high & (0x1 << (io_num -
32))) : (gpio_pin_mask & (0x1 << io_num)));

        if (bit_valid && (io_reg = GPIO_PIN_REG[io_num])) {
            if (pGPIOConfig->GPIO_Mode == GPIO_Mode_Input) {
                SET_PERI_REG_MASK(io_reg, FUN_IE);
            }

            //for ESP32 function 2 of every pad is allways GPIO func
            PIN_FUNC_SELECT(io_reg, 2);

            if (pGPIOConfig->GPIO_Pullup) {
                PIN_PULLUP_EN(io_reg);
            } else {
                PIN_PULLUP_DIS(io_reg);
            }

            if (pGPIOConfig->GPIO_Pulldown) {
                PIN_PULLDWN_EN(io_reg);
            } else {
                PIN_PULLDWN_DIS(io_reg);
            }

            if (pGPIOConfig->GPIO_Mode == GPIO_Mode_Out_OD) {
                portENTER_CRITICAL(&g_gpio_mux);

                pin_reg = GPIO_REG_READ(GPIO_PIN_ADDR(io_num));
                //pin_reg &= (~GPIO_GPIO_PIN0_PAD_DRIVER);
                pin_reg |= GPIO_GPIO_PIN0_PAD_DRIVER;
                GPIO_REG_WRITE(GPIO_PIN_ADDR(io_num), pin_reg);

                portEXIT_CRITICAL(&g_gpio_mux);
            }

            gpio_pin_intr_state_set(io_num, pGPIOConfig->GPIO_IntrType);
        }

        io_num++;
    } while (io_num < GPIO_PIN_COUNT);
}*/

void gpio_output_sigmadelta_enable(uint32 gpio_num, uint32 sigma_num, uint32 prescale)
{
    if (sigma_num >= 8)
    {
        return;
    }

    SET_PERI_REG_BITS(SIGMADELTA0 + sigma_num * 4, SIGMADELTA_SD0_PRESCALE, prescale, SIGMADELTA_SD0_PRESCALE_S);
    gpio_matrix_out(gpio_num, GPIO_SD0_OUT_IDX + sigma_num, false, false);
}

void gpio_output_sigmadelta_disable(uint32 gpio_num) { gpio_matrix_out(gpio_num, 0x80, false, false); }

/*
 * Change GPIO pin output by setting, clearing, or disabling pins.
 * In general, it is expected that a bit will be set in at most one
 * of these masks.  If a bit is clear in all masks, the output state
 * remains unchanged.
 *
 * There is no particular ordering guaranteed; so if the order of
 * writes is significant, calling code should divide a single call
 * into multiple calls.
 *
 * This function only config GPIO0-GPIO31, If you want to config GPIO32-GPIO39,
 * use gpio_output_conf_high
 *
 */
void gpio_output_conf(uint32 set_mask, uint32 clear_mask, uint32 enable_mask, uint32 disable_mask)
{
    GPIO_REG_WRITE(GPIO_OUT_W1TS, set_mask);
    GPIO_REG_WRITE(GPIO_OUT_W1TC, clear_mask);
    GPIO_REG_WRITE(GPIO_ENABLE_W1TS, enable_mask);
    GPIO_REG_WRITE(GPIO_ENABLE_W1TC, disable_mask);
}

/*
 * Change GPIO pin output by setting, clearing, or disabling pins.
 * In general, it is expected that a bit will be set in at most one
 * of these masks.  If a bit is clear in all masks, the output state
 * remains unchanged.
 *
 * There is no particular ordering guaranteed; so if the order of
 * writes is significant, calling code should divide a single call
 * into multiple calls.
 *
 * This function only config GPIO32-GPIO39, If you want to config GPIO0-GPIO31,
 * use gpio_output_conf
 *
 */
void gpio_output_conf_high(uint32 set_mask, uint32 clear_mask, uint32 enable_mask, uint32 disable_mask)
{
    GPIO_REG_WRITE(GPIO_OUT1_W1TS, set_mask);
    GPIO_REG_WRITE(GPIO_OUT1_W1TC, clear_mask);
    GPIO_REG_WRITE(GPIO_ENABLE1_W1TS, enable_mask);
    GPIO_REG_WRITE(GPIO_ENABLE1_W1TC, disable_mask);
}

/*
 * Sample the value of GPIO input pins and returns a bitmask.
 *
 * Only GPIO0-GPIO31
 */
uint32 gpio_input_get(void) { return GPIO_REG_READ(GPIO_IN); }

/*
 * Sample the value of GPIO input pins and returns a bitmask.
 *
 * Only GPIO32-GPIO39
 */
uint32 gpio_input_get_high(void) { return GPIO_REG_READ(GPIO_IN1); }

// we do not support sleep by now
#if 0
/*
  only highlevel and lowlevel intr can use for wakeup
*/
void gpio_pin_wakeup_enable(uint32 i, GPIO_INT_TYPE intr_state)
{
    uint32 pin_reg;

    if ((intr_state == GPIO_PIN_INTR_LOLEVEL) || (intr_state == GPIO_PIN_INTR_HILEVEL)) {
        portENTER_CRITICAL(&g_gpio_mux);

        pin_reg = GPIO_REG_READ(GPIO_PIN_ADDR(i));
        pin_reg &= (~GPIO_PIN_INT_TYPE_MASK);
        pin_reg |= (intr_state << GPIO_PIN_INT_TYPE_LSB);
        pin_reg |= GPIO_PIN_WAKEUP_ENABLE_SET(GPIO_WAKEUP_ENABLE);
        GPIO_REG_WRITE(GPIO_PIN_ADDR(i), pin_reg);

        portEXIT_CRITICAL(&g_gpio_mux);
    }
}

void gpio_pin_wakeup_disable(void)
{
    uint8  i;
    uint32 pin_reg;

    for (i = 0; i < GPIO_PIN_COUNT; i++) {
        pin_reg = GPIO_REG_READ(GPIO_PIN_ADDR(i));

        if (pin_reg & GPIO_PIN_WAKEUP_ENABLE_MASK) {
            pin_reg &= (~GPIO_PIN_INT_TYPE_MASK);
            pin_reg |= (GPIO_PIN_INTR_DISABLE << GPIO_PIN_INT_TYPE_LSB);
            pin_reg &= ~(GPIO_PIN_WAKEUP_ENABLE_SET(GPIO_WAKEUP_ENABLE));
            GPIO_REG_WRITE(GPIO_PIN_ADDR(i), pin_reg);
        }
    }
}
#endif

void gpio_pin_intr_state_set(uint32 i, GPIO_INT_TYPE intr_state)
{
    uint32 pin_reg;

    portENTER_CRITICAL(&g_gpio_mux);

    pin_reg = GPIO_REG_READ(GPIO_PIN_ADDR(i));
    pin_reg &= (~GPIO_PIN_INT_TYPE_MASK);
    pin_reg |= (intr_state << GPIO_PIN_INT_TYPE_LSB);
    pin_reg &= (~GPIO_GPIO_PIN0_INT_ENA);
    pin_reg |= (BIT(2) << GPIO_GPIO_PIN0_INT_ENA_S);
    GPIO_REG_WRITE(GPIO_PIN_ADDR(i), pin_reg);

    portEXIT_CRITICAL(&g_gpio_mux);
}
