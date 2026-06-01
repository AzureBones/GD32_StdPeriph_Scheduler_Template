/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#include "mcu_cmic_gd32f470vet6.h"

__IO uint16_t tx_count = 0;

ring_buffer_t uart0_rb;
static uint8_t uart0_rb_pool[512];

void app_ringbuffer_init(void)
{
    ring_buffer_init(&uart0_rb, uart0_rb_pool, sizeof(uart0_rb_pool));
}

int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;
    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    for(tx_count = 0; tx_count < len; tx_count++){
        usart_data_transmit(usart_periph, buffer[tx_count]);
        while(RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    }

    return len;
}

void uart_task(void)
{
    uint8_t buf[512];
    uint32_t len;

    len = ring_buffer_available(&uart0_rb);
    if(len == 0) return;

    if(len > sizeof(buf)){
        len = sizeof(buf);
    }
    ring_buffer_read(&uart0_rb, buf, len);

    shell_process(buf, (uint16_t)len);
}
