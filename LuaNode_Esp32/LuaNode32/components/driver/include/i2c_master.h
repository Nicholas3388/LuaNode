#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

#include "c_types.h"

#define I2C_MASTER_SDA_MUX (pin_mux[sda])
#define I2C_MASTER_SCL_MUX (pin_mux[scl])
#define I2C_MASTER_SDA_GPIO (pinSDA)
#define I2C_MASTER_SCL_GPIO (pinSCL)
#define I2C_MASTER_SDA_FUNC (pin_func[sda])
#define I2C_MASTER_SCL_FUNC (pin_func[scl])

// #define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_GPIO2_U
// #define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_MTDO_U
// #define I2C_MASTER_SDA_GPIO 2
// #define I2C_MASTER_SCL_GPIO 15
// #define I2C_MASTER_SDA_FUNC FUNC_GPIO2
// #define I2C_MASTER_SCL_FUNC FUNC_GPIO15

// #define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_GPIO2_U
// #define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_MTMS_U
// #define I2C_MASTER_SDA_GPIO 2
// #define I2C_MASTER_SCL_GPIO 14
// #define I2C_MASTER_SDA_FUNC FUNC_GPIO2
// #define I2C_MASTER_SCL_FUNC FUNC_GPIO14

//#define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_GPIO2_U
//#define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_GPIO0_U
//#define I2C_MASTER_SDA_GPIO 2
//#define I2C_MASTER_SCL_GPIO 0
//#define I2C_MASTER_SDA_FUNC FUNC_GPIO2
//#define I2C_MASTER_SCL_FUNC FUNC_GPIO0


#define I2C_MASTER_SDA_HIGH_SCL_HIGH()  \
    gpio_output_conf(1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_HIGH_SCL_LOW()  \
    gpio_output_conf(1<<I2C_MASTER_SDA_GPIO, 1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_LOW_SCL_HIGH()  \
    gpio_output_conf(1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

#define I2C_MASTER_SDA_LOW_SCL_LOW()  \
    gpio_output_conf(0, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 1<<I2C_MASTER_SDA_GPIO | 1<<I2C_MASTER_SCL_GPIO, 0)

//void i2c_master_gpio_init(uint8 sda, uint8 scl);
void i2c_master_init(void);

#define i2c_master_wait    os_delay_us
//void i2c_master_stop(void);
//void i2c_master_start(void);
void i2c_master_setAck(uint8 level);
uint8 i2c_master_getAck(void);
//uint8 i2c_master_readByte(void);
//void i2c_master_writeByte(uint8 wrdata);

bool i2c_master_checkAck(void);
void i2c_master_send_ack(void);
void i2c_master_send_nack(void);

uint8 i2c_master_get_pinSDA();
uint8 i2c_master_get_pinSCL();

#endif