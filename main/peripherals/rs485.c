#include <driver/gpio.h>
#include <driver/uart.h>
#include "hardwareprofile.h"


#define MB_PORTNUM 1
// Timeout threshold for UART = number of symbols (~10 tics) with unchanged
// state on receive pin
#define ECHO_READ_TOUT (3)     // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks
#define MODBUS_TIMEOUT 10


void rs485_init(int baud_rate) {
    uart_config_t uart_config = {
        .baud_rate           = baud_rate,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(MB_PORTNUM, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(MB_PORTNUM, MB_UART_TXD, MB_UART_RXD, MB_DERE, -1));
    ESP_ERROR_CHECK(uart_driver_install(MB_PORTNUM, 256, 256, 10, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(MB_PORTNUM, UART_MODE_RS485_HALF_DUPLEX));
    ESP_ERROR_CHECK(uart_set_rx_timeout(MB_PORTNUM, ECHO_READ_TOUT));
}


int rs485_read(uint8_t *buffer, size_t len) {
    return uart_read_bytes(MB_PORTNUM, buffer, len, pdMS_TO_TICKS(MODBUS_TIMEOUT));
}


int rs485_write(uint8_t *buffer, size_t len) {
    return uart_write_bytes(MB_PORTNUM, buffer, len);
}


void rs485_flush(void) {
    uart_flush_input(MB_PORTNUM);
}
