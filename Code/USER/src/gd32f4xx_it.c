/*!
    \file    gd32f4xx_it.c
    \brief   interrupt service routines

    \version 2024-12-20, V3.3.1, firmware for GD32F4xx
*/

/*
    Copyright (c) 2024, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx_it.h"
#include "main.h"
#include "mcu_cmic_gd32f470vet6.h"
#include "systick.h"
#include "sdio_sdcard.h"
#include "string.h"

extern uint8_t usart0_rxbuffer[512];
extern volatile uint8_t adc_dma_done;

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    /* if NMI exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    /* if Hard Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
    /* if SVC exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
    /* if DebugMon exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
    /* if PendSV exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles USART interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void USART0_IRQHandler(void)
{
    uint32_t rx_len;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE)){
        /* clear IDLE flag */
        usart_data_receive(USART0);
        dma_channel_disable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);

        /* number of data received */
        rx_len = sizeof(usart0_rxbuffer) - dma_transfer_number_get(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
        if((rx_len > 0U) && (rx_len <= sizeof(usart0_rxbuffer))){
            ring_buffer_write(&uart0_rb, usart0_rxbuffer, rx_len);
        }
        memset(usart0_rxbuffer, 0, sizeof(usart0_rxbuffer));

        /* disable DMA and reconfigure */
        dma_flag_clear(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, DMA_FLAG_FTF);
        dma_transfer_number_config(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, sizeof(usart0_rxbuffer));
        dma_channel_enable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
    }
}

void DMA1_Channel0_IRQHandler(void)
{
    if(RESET != dma_interrupt_flag_get(DMA1, DMA_CH0, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(DMA1, DMA_CH0, DMA_INT_FLAG_FTF);
        adc_dma_done = 1;
        /* stop ADC DMA requests first (mirrors HAL_ADC_Stop_DMA) */
        adc_dma_mode_disable(ADC0);
        /* restart DMA controller */
        dma_channel_disable(DMA1, DMA_CH0);
        dma_transfer_number_config(DMA1, DMA_CH0, 1000);
        dma_channel_enable(DMA1, DMA_CH0);
        /* re-enable ADC DMA requests (mirrors HAL_ADC_Start_DMA) */
        adc_dma_mode_enable(ADC0);
    }
}

void EXTI0_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_0)) {
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void SDIO_IRQHandler(void)
{
    sd_interrupts_process();
}

/*!
    \brief    this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    delay_decrement();
}
