#include "mcu_cimc_gd32f470vet6.h"

extern uint16_t adc_value[1000];
extern uint16_t convertarr[CONVERT_NUM];
extern volatile uint8_t adc_dma_done;

void adc_task(void)
{
    if(adc_dma_done == 0) {
        return;
    }
    adc_dma_done = 0;

    /* adc_value[0..999]: 500 groups, interleaved CH10/CH12
     * even index = CH10 (PC0), odd index = CH12 (PC2) */
    convertarr[0] = adc_value[0];
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, convertarr[0]);
}

