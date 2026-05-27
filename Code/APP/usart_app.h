#ifndef __USART_APP_H__
#define __USART_APP_H__

#include "stdint.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ring_buffer_t uart0_rb;

void app_ringbuffer_init(void);
int my_printf(uint32_t usart_periph, const char *format, ...);
void uart_task(void);

#ifdef __cplusplus
}
#endif

#endif
