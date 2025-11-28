
#pragma once 

#include_next <mcuconf.h>

#undef STM32_HSE_ENABLED
#define STM32_HSE_ENABLED TRUE 

#undef STM32_PLLSRC
#define STM32_PLLSRC STM32_PLLSRC_HSE

// 8 / 1 = 8мгц кварц
#undef STM32_PLLM_VALUE
#define STM32_PLLM_VALUE 1 

// 8 * 40 = 320мгц
#undef STM32_PLLN_VALUE
#define STM32_PLLN_VALUE 40

// 320 / 2 = 160мгц частота контроллера
#undef STM32_PLLR_VALUE
#define STM32_PLLR_VALUE 2

#undef STM32_ADC_USE_ADC2
#define STM32_ADC_USE_ADC2 TRUE

