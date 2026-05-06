#ifndef AUDIOMARK_SSE320_PLATFORM_IRQ_H
#define AUDIOMARK_SSE320_PLATFORM_IRQ_H

#include_next "platform_irq.h"

#if defined(SSE_320_FPGA)

#define UARTRX0_IRQn ((IRQn_Type)49)
#define ISP_IRQn     ((IRQn_Type)33)

#endif

#endif
