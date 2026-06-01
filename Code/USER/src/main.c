/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/05/15
* Note:
*/
#include "mcu_cmic_gd32f470vet6.h"

int main(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
#endif
    systick_config();
    init_cycle_counter(false);
    delay_ms(200); // Wait download if SWIO be set to GPIO

#ifdef __FIRMWARE_VERSION_DEFINE
    fw_ver = gd32f4xx_firmware_version_get();
#endif /* __FIRMWARE_VERSION_DEFINE */

    bsp_led_init();
    bsp_btn_init();
    bsp_oled_init();
    bsp_gd25qxx_init();
    bsp_usart_init();

    my_printf(DEBUG_USART, "BOOT: start\r\n");

    my_printf(DEBUG_USART, "BOOT: gd30 init...\r\n");
    bsp_gd30ad3344_init();
    my_printf(DEBUG_USART, "BOOT: gd30 done\r\n");

    my_printf(DEBUG_USART, "BOOT: adc init...\r\n");
    bsp_adc_init();
    my_printf(DEBUG_USART, "BOOT: adc done\r\n");

    my_printf(DEBUG_USART, "BOOT: dac init...\r\n");
    bsp_dac_init();
    my_printf(DEBUG_USART, "BOOT: dac done\r\n");

    my_printf(DEBUG_USART, "BOOT: rtc init...\r\n");
    bsp_rtc_init();
    my_printf(DEBUG_USART, "BOOT: rtc done\r\n");

    sd_fatfs_init();
    app_btn_init();
    app_ringbuffer_init();

    my_printf(DEBUG_USART, "BOOT: oled init...\r\n");
    OLED_Init();
    my_printf(DEBUG_USART, "BOOT: oled done\r\n");

    test_spi_flash();
#if SD_FATFS_DEMO_ENABLE
    sd_fatfs_test();
#else
    my_printf(DEBUG_USART, "BOOT: sd_fatfs_test skipped (SD_FATFS_DEMO_ENABLE=0)\r\n");
#endif

    shell_init();
    scheduler_init();
    while(1) {
        scheduler_run();
    }
}

#ifdef GD_ECLIPSE_GCC
/* retarget the C library printf function to the USART, in Eclipse GCC environment */
int __io_putchar(int ch)
{
    usart_data_transmit(EVAL_COM0, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE));
    return ch;
}
#else
/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    usart_data_transmit(USART0, (uint8_t)ch);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return ch;
}
#endif /* GD_ECLIPSE_GCC */
