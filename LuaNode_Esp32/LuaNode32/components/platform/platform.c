// Platform-dependent functions

#include "platform.h"

#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "extras/soc_ext.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mygpio.h"
#include "platform_partition.h"
#include "rom/ets_sys.h"
#include "rom/gpio.h"
#include "rom/spi_flash.h"
#include "rom/uart.h"
#include "soc/soc.h"
#include "soc/uart_struct.h"
#include "user_config.h"
// Platform specific includes

#include "gpio16.h"
#include "i2c_master.h"
#include "rom.h"
//#include "spi_api.h"
#include <stdio.h>
#include <stdlib.h>

#include "pin_map.h"

uint16_t flash_safe_get_sec_num(void);

#define FLASH_SEC_NUM (flash_safe_get_sec_num())
#define SYS_PARAM_SEC_NUM 4
#define SYS_PARAM_SEC_START (FLASH_SEC_NUM - SYS_PARAM_SEC_NUM)

#define DATA_LENGTH 512                        /*!<Data buffer length for test buffer*/
#define I2C_MASTER_FREQ_HZ 100000              /*!< I2C master clock frequency */
#define I2C_SLAVE_TX_BUF_LEN (2 * DATA_LENGTH) /*!<I2C slave tx buffer size */
#define I2C_SLAVE_RX_BUF_LEN (2 * DATA_LENGTH) /*!<I2C slave rx buffer size */
#define I2C_MASTER_TX_BUF_DISABLE 0            /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0            /*!< I2C master do not need buffer */
#define ACK_CHECK_EN 0x1                       /*!< I2C master will check ack from slave*/
#define ACK_VAL 0x0                            /*!< I2C ack value */
#define NACK_VAL 0x1                           /*!< I2C nack value */

#define BUF_SIZE (1024)

static xTaskHandle xHandle;
static QueueHandle_t uart1_queue;
static xQueueHandle gpio_evt_queue = NULL;
static const char *TAG = "platform";
bool isr_installed = false;

// static void pwms_init();
void platform_gpio_init();

int platform_init()
{
    // gpio init
    platform_gpio_init();
    return PLATFORM_OK;
}

// ****************************************************************************
// KEY_LED functions
uint8_t platform_key_led(uint8_t level)
{
    uint8_t temp;
    gpio16_output_set(1);  // set to high first, for reading key low level
    gpio16_input_conf();
    temp = gpio16_input_get();
    gpio16_output_conf();
    gpio16_output_set(level);
    return temp;
}

// ****************************************************************************
// GPIO functions
#ifdef GPIO_INTERRUPT_ENABLE
extern void lua_gpio_unref(unsigned pin);
extern void gpio_intr_callback(unsigned pin, unsigned level);
#endif

void gpio_task(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            // printf("GPIO[%d] intr, val: %d\n", io_num,
            // gpio_get_level(io_num));
            gpio_intr_callback((int)io_num, gpio_get_level(io_num));
        }
    }
}

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

int platform_gpio_mode(unsigned pin, unsigned mode, unsigned type)
{
    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = (mode == PLATFORM_GPIO_INT ? type : GPIO_PIN_INTR_DISABLE);
    // set as output mode
    io_conf.mode = mode;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1 << pin);
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = (mode == PLATFORM_GPIO_INT ? 1 : 0);
    // io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);

    if (mode == PLATFORM_GPIO_INT)
    {
        // change gpio intrrupt type for one pin
        gpio_set_intr_type(pin, type);
        // install gpio isr service
        if (!isr_installed)
        {
            gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
            isr_installed = true;
        }
        // hook isr handler for specific gpio pin
        gpio_isr_handler_add(pin, gpio_isr_handler, (void *)pin);
    }
    return 1;
}

void platform_gpio_isr_uninstall(void) { isr_installed = false; }

int platform_gpio_write(unsigned pin, unsigned level) { return gpio_set_level(pin, level); }

int platform_gpio_read(unsigned pin) { return gpio_get_level(pin); }

#ifdef GPIO_INTERRUPT_ENABLE
static void platform_gpio_intr_dispatcher(platform_gpio_intr_handler_fn_t cb)
{
    uint8 i, level;
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    for (i = 0; i < GPIO_PIN_NUM; i++)
    {
        if (pin_int_type[i] && (gpio_status & BIT(pin_num[i])))
        {
            // disable interrupt
            gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[i]), GPIO_PIN_INTR_DISABLE);
            // clear interrupt status
            GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(pin_num[i]));
            level = 0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[i]));
            if (cb)
            {
                cb(i, level);
            }
            gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[i]), pin_int_type[i]);
        }
    }
}

void platform_gpio_init(void)
{
    // ETS_GPIO_INTR_ATTACH(platform_gpio_intr_dispatcher, cb);
    // ETS_GPIO_INTR_ATTACH(cb, NULL);
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    // start gpio task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
}

int platform_gpio_intr_init(unsigned pin, GPIO_INT_TYPE type)
{
    if (pin >= NUM_GPIO)
        return -1;
    ETS_GPIO_INTR_DISABLE();
    // clear interrupt status
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin_num[pin]));
    pin_int_type[pin] = type;
    // enable interrupt
    gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), type);
    ETS_GPIO_INTR_ENABLE();
    return 0;
}
#endif

// ****************************************************************************
// UART
// TODO: Support timeouts.

static void my_uart_task(void *pvParameters)
{
    int uart_num = (int)pvParameters;
    uart_event_t event;
    size_t buffered_size;
    uint8_t *dtmp = (uint8_t *)malloc(BUF_SIZE);
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart1_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            ESP_LOGI(TAG, "uart[%d] event:", uart_num);
            switch (event.type)
            {
                // Event of UART receving data
                /*We'd better handler data event fast, there would be much more
                data events than other types of events. If we take too much time
                on data event, the queue might be full. in this example, we
                don't process data in event, but read data outside.*/
                case UART_DATA:
                    uart_get_buffered_data_len(uart_num, &buffered_size);
                    ESP_LOGI(TAG, "data, len: %d; buffered len: %d", event.size, buffered_size);

                    uint8_t data[512] = {0};
                    int len = uart_read_bytes(uart_num, data, BUF_SIZE, 100 / portTICK_RATE_MS);
                    if (len > 0)
                    {
                        ESP_LOGI(TAG, "uart read : %d", len);
                        uart_write_bytes(uart_num, (const char *)data, len);
                    }
                    break;
                // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow\n");
                    // If fifo overflow happened, you should consider adding
                    // flow control for your application. We can read data out
                    // out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full\n");
                    // If buffer full happened, you should consider encreasing
                    // your buffer size We can read data out out the buffer, or
                    // directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                // Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break\n");
                    break;
                // Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error\n");
                    break;
                // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error\n");
                    break;
                // UART_PATTERN_DET
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected\n");
                    break;
                // Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

uint32_t platform_uart_setup(unsigned id, uint32_t baud, int databits, int parity, int stopbits, int flow, int txd,
                             int rxd, int rts, int cts)
{
    int uart_num = id;
    uart_config_t uart_config;

    uart_config.baud_rate = baud;
    uart_config.data_bits = databits;
    uart_config.parity = parity;
    uart_config.stop_bits = stopbits;
    uart_config.flow_ctrl = flow;
    uart_config.rx_flow_ctrl_thresh = 122;

    // Configure UART1 parameters
    uart_param_config(uart_num, &uart_config);
    // Set UART1 pins(TX: IO4, RX: I05, RTS: IO18, CTS: IO19)
    uart_set_pin(uart_num, txd, rxd, rts, cts);
    // Install UART driver( We don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart1_queue, 0);

    uart_enable_pattern_det_intr(uart_num, '+', 3, 10000, 10, 10);
    // Create a task to handler UART event from ISR
    xTaskCreate(my_uart_task, "my_uart_task", 2048, (void *)uart_num, 16, &xHandle);
    // xTaskCreate(my_recv_task, "my_recv_task", 2048, (void*)uart_num, 16,
    // &recvHandle ); process data

    return baud;
}

// if set=1, then alternate serial output pins are used. (15=rx, 13=tx)
void platform_uart_alt(int set)
{
    // deprecated
    return;
}

// Send: version with and without mux
void platform_uart_send(unsigned id, u8 *data, unsigned len)
{
    // uart_tx_one_char(data);
    uart_write_bytes(id, (const char *)data, len);
    // printf("%c", data);
}

void platform_uart_uninstall(uint8_t uart_num)
{
    uart_driver_delete(uart_num);
    vTaskDelete(xHandle);
}

int platform_uart_exists(unsigned id)
{
    if (id < 0 || id > 3)
    {
        return 0;
    }
    return 1;
}

// ****************************************************************************
// PWMs

static uint16_t pwms_duty[NUM_PWM] = {0};

/*static void pwms_init()
{
  int i;
  for(i=0;i<NUM_PWM;i++){
    pwms_duty[i] = DUTY(0);
  }
  pwm_init(500, NULL);
  // NODE_DBG("Function pwms_init() is called.\n");
}*/

// Return the PWM clock
// NOTE: Can't find a function to query for the period set for the timer,
// therefore using the struct.
// This may require adjustment if driver libraries are updated.
uint32_t platform_pwm_get_clock(void)
{
    // NODE_DBG("Function platform_pwm_get_clock() is called.\n");
    uint32_t freq = ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
    return (uint32_t)freq;
}

// Set the PWM clock
uint32_t platform_pwm_set_clock(unsigned pin, uint32_t clock)
{
    // NODE_DBG("Function platform_pwm_set_clock() is called.\n");
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, clock);
    return (uint32_t)clock;
}

uint32_t platform_pwm_get_duty(unsigned channel)
{
    // NODE_DBG("Function platform_pwm_get_duty() is called.\n");
    int duty = ledc_get_duty(LEDC_HIGH_SPEED_MODE, channel);
    if (duty)
    {
        return duty;
    }
    return 0;
}

// Set the PWM duty
uint32_t platform_pwm_set_duty(unsigned channel, uint32_t duty)
{
    // NODE_DBG("Function platform_pwm_set_duty() is called.\n");
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel,
                  duty);  // the new duty is not valid, until ledc_update_duty is called
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
    return duty;
}

uint32_t platform_pwm_setup(unsigned pin, uint32_t frequency, unsigned duty, unsigned channel)
{
    ledc_timer_config_t ledc_timer = {// set timer counter bit number
                                      .bit_num = LEDC_TIMER_13_BIT,
                                      // set frequency of pwm
                                      .freq_hz = frequency,
                                      // timer mode,
                                      .speed_mode = LEDC_HIGH_SPEED_MODE,
                                      // timer index
                                      .timer_num = LEDC_TIMER_0};
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {// set LEDC channel 0
                                          .channel = channel,
                                          // set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
                                          .duty = duty,
                                          // GPIO number
                                          .gpio_num = pin,
                                          // GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
                                          .intr_type = LEDC_INTR_FADE_END,
                                          // set LEDC mode, from ledc_mode_t
                                          .speed_mode = LEDC_HIGH_SPEED_MODE,
                                          // set LEDC timer source, if different channel use one timer,
                                          // the frequency and bit_num of these channels should be the same
                                          .timer_sel = LEDC_TIMER_0};
    // set the configuration
    ledc_channel_config(&ledc_channel);

    return frequency;
}

void platform_pwm_close(unsigned pin)
{
    // NODE_DBG("Function platform_pwm_stop() is called.\n");
    // deprecated
}

void platform_pwm_start(void)
{
    // NODE_DBG("Function platform_pwm_start() is called.\n");
    // deprecated
}

void platform_pwm_stop(unsigned channel)
{
    // NODE_DBG("Function platform_pwm_stop() is called.\n");
    int output_idle_level = 0;
    ledc_stop(LEDC_HIGH_SPEED_MODE, channel, output_idle_level);
}

// *****************************************************************************
// I2C platform interface
void platform_i2c_setup(uint8_t mode, uint8_t port, uint8_t scl, uint8_t sda, uint8_t addr)
{
    i2c_config_t conf;
    conf.mode = mode;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    if (mode == I2C_MODE_MASTER)
    {
        conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    }
    else
    {
        conf.slave.addr_10bit_en = 0;
        conf.slave.slave_addr = addr;
    }

    i2c_param_config(port, &conf);

    if (mode == I2C_MODE_MASTER)
    {
        i2c_driver_install(port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    }
    else
    {
        i2c_driver_install(port, conf.mode, I2C_SLAVE_RX_BUF_LEN, I2C_SLAVE_TX_BUF_LEN, 0);
    }
}

void platform_i2c_send_start(unsigned id)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
}

void platform_i2c_send_stop(unsigned id)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_stop(cmd);
}

#if 0
int platform_i2c_send_address( unsigned id, uint16_t address, int direction ){
  // Convert enum codes to R/w bit value.
  // If TX == 0 and RX == 1, this test will be removed by the compiler
  if ( ! ( PLATFORM_I2C_DIRECTION_TRANSMITTER == 0 &&
           PLATFORM_I2C_DIRECTION_RECEIVER == 1 ) ) {
    direction = ( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ) ? 0 : 1;
  }

  i2c_master_writeByte( (uint8_t) ((address << 1) | direction ));
  // Low-level returns nack (0=acked); we return ack (1=acked).
  return ! i2c_master_getAck();
}
#endif

int platform_i2c_send_byte(unsigned mode, unsigned port, unsigned addr, uint8_t *data, uint32_t len)
{
    if (mode == I2C_MODE_SLAVE)
    {
        size_t d_size = i2c_slave_write_buffer(port, data, len, 1000 / portTICK_RATE_MS);
        return d_size;
    }
    else
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
        i2c_master_write(cmd, data, len, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        return ret;
    }
    return 999;
}

int platform_i2c_recv_byte(uint8_t mode, uint8_t port, uint8_t addr, uint8_t *data, uint32_t len)
{
    if (mode == I2C_MODE_SLAVE)
    {
        int size = i2c_slave_read_buffer(port, data, len, 1000 / portTICK_RATE_MS);
        return size;
    }
    else
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
        if (len > 1)
        {
            i2c_master_read(cmd, data, len - 1, ACK_VAL);
        }
        i2c_master_read_byte(cmd, data + len - 1, NACK_VAL);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        return ret;
    }
    return len;
}

void platform_i2c_uninstall(uint8_t i2c_num) { i2c_driver_delete(i2c_num); }

// *****************************************************************************
// SPI platform interface
uint32_t platform_spi_setup(uint8_t id, int mode, unsigned cpol, unsigned cpha, uint32_t clock_div)
{
    // spi_master_init( id, cpol, cpha, clock_div );
    return 1;
}

int platform_spi_send(uint8_t id, uint8_t bitlen, spi_data_type data)
{
    if (bitlen > 32)
        return PLATFORM_ERR;

    // spi_mast_transaction( id, 0, 0, bitlen, data, 0, 0, 0 );
    return PLATFORM_OK;
}

spi_data_type platform_spi_send_recv(uint8_t id, uint8_t bitlen, spi_data_type data)
{
    if (bitlen > 32)
        return 0;

    // spi_mast_set_mosi( id, 0, bitlen, data );
    // spi_mast_transaction( id, 0, 0, 0, 0, bitlen, 0, -1 );
    // return spi_mast_get_miso( id, 0, bitlen );
    return PLATFORM_OK;
}

int platform_spi_set_mosi(uint8_t id, uint8_t offset, uint8_t bitlen, spi_data_type data)
{
    if (offset + bitlen > 512)
        return PLATFORM_ERR;

    // spi_mast_set_mosi( id, offset, bitlen, data );

    return PLATFORM_OK;
}

spi_data_type platform_spi_get_miso(uint8_t id, uint8_t offset, uint8_t bitlen)
{
    if (offset + bitlen > 512)
        return 0;

    // return spi_mast_get_miso( id, offset, bitlen );
    return PLATFORM_OK;
}

int platform_spi_transaction(uint8_t id, uint8_t cmd_bitlen, spi_data_type cmd_data, uint8_t addr_bitlen,
                             spi_data_type addr_data, uint16_t mosi_bitlen, uint8_t dummy_bitlen, int16_t miso_bitlen)
{
    if ((cmd_bitlen > 16) || (addr_bitlen > 32) || (mosi_bitlen > 512) || (dummy_bitlen > 256) || (miso_bitlen > 512))
        return PLATFORM_ERR;

    // spi_mast_transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data,
    // mosi_bitlen, dummy_bitlen, miso_bitlen );

    return PLATFORM_OK;
}

// ****************************************************************************
// Flash access functions
uint32_t platform_get_flash_size(uint32_t phy_start_addr)
{
    return ((SYS_PARAM_SEC_START * INTERNAL_FLASH_SECTOR_SIZE) - phy_start_addr);
}

static uint32_t flashh_find_sector(uint32_t address, uint32_t *pstart, uint32_t *pend)
{
    // All the sectors in the flash have the same size, so just align the
    // address
    uint32_t sect_id = address / INTERNAL_FLASH_SECTOR_SIZE;

    if (pstart)
    {
        *pstart = sect_id * INTERNAL_FLASH_SECTOR_SIZE;
    }
    if (pend)
    {
        *pend = (sect_id + 1) * INTERNAL_FLASH_SECTOR_SIZE - 1;
    }
    return sect_id;
}

uint32_t platform_flash_mapped2phys(uint32_t mapped_addr)
{
    uint32_t cache_ctrl = DPORT_READ_PERI_REG(CACHE_FLASH_CTRL_REG);
    if (!(cache_ctrl & CACHE_FLASH_ACTIVE))
        return -1;
    bool b0 = (cache_ctrl & CACHE_FLASH_MAPPED0) ? 1 : 0;
    bool b1 = (cache_ctrl & CACHE_FLASH_MAPPED1) ? 1 : 0;
    uint32_t meg = (b1 << 1) | b0;
    return mapped_addr - INTERNAL_FLASH_MAPPED_ADDRESS + meg * 0x100000;
}

uint32_t platform_flash_get_first_free_block_address(uint32_t *psect)
{
    // Round the total used flash size to the closest flash block address
    uint32_t start, end, sect;
    uint32_t _flash_used_end = 0x110000;
    printf("_flash_used_end:%08x\n", (uint32_t)_flash_used_end);
    if (_flash_used_end > 0)
    {  // find the used sector
        sect = flashh_find_sector(platform_flash_mapped2phys((uint32_t)_flash_used_end - 1), NULL, &end);
        if (psect)
        {
            *psect = sect + 1;
        }
        return end + 1;
    }
    else
    {
        sect = flashh_find_sector(0, &start, NULL);  // find the first free sector
        if (psect)
        {
            *psect = sect;
        }
        return start;
    }
}
