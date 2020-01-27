#include "rom/ets_sys.h"
//#include "osapi.h"
#include "extras/soc_ext.h"
#include "gpio16.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc.h"

void gpio16_output_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) |
                       (uint32)0x1);  // mux configuration for XPD_DCDC to output rtc_gpio0

    WRITE_PERI_REG(
        RTC_GPIO_CONF,
        (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);  // mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);  // out enable
}

void gpio16_output_set(uint8 value)
{
    WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)(value & 1));
}

void gpio16_input_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1);  // mux configuration for XPD_DCDC
                                                                                    // and rtc_gpio0 connection

    WRITE_PERI_REG(
        RTC_GPIO_CONF,
        (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);  // mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);  // out disable
}

uint8 gpio16_input_get(void) { return (uint8)(READ_PERI_REG(RTC_GPIO_IN_DATA) & 1); }
