#ifndef AUDIOMARK_SSE320_PLATFORM_BASE_ADDRESS_H
#define AUDIOMARK_SSE320_PLATFORM_BASE_ADDRESS_H

#include_next "platform_base_address.h"

#if defined(SSE_320_FPGA)

#undef GPIO0_CMSDK_BASE_NS
#undef GPIO1_CMSDK_BASE_NS
#undef GPIO2_CMSDK_BASE_NS
#undef GPIO3_CMSDK_BASE_NS
#define GPIO0_CMSDK_BASE_NS              0x41100000
#define GPIO1_CMSDK_BASE_NS              0x41101000
#define GPIO2_CMSDK_BASE_NS              0x41102000
#define GPIO3_CMSDK_BASE_NS              0x41103000

#undef FPGA_DDR4_EEPROM_BASE_NS
#undef FPGA_SCC_BASE_NS
#undef FPGA_I2S_BASE_NS
#undef FPGA_IO_BASE_NS
#undef UART0_BASE_NS
#undef UART1_BASE_NS
#undef UART2_BASE_NS
#undef UART3_BASE_NS
#undef UART4_BASE_NS
#undef UART5_BASE_NS
#define FPGA_DDR4_EEPROM_BASE_NS         0x49208000
#define FPGA_SCC_BASE_NS                 0x49300000
#define FPGA_I2S_BASE_NS                 0x49301000
#define FPGA_IO_BASE_NS                  0x49302000
#define UART0_BASE_NS                    0x49303000
#define UART1_BASE_NS                    0x49304000
#define UART2_BASE_NS                    0x49305000
#define UART3_BASE_NS                    0x49306000
#define UART4_BASE_NS                    0x49307000
#define UART5_BASE_NS                    0x49308000

#undef HDLCD_BASE_S
#define HDLCD_BASE_S                     0x5930A000

#undef FPGA_SCC_BASE_S
#undef FPGA_I2S_BASE_S
#undef FPGA_IO_BASE_S
#undef UART0_BASE_S
#undef UART1_BASE_S
#undef UART2_BASE_S
#undef UART3_BASE_S
#undef UART4_BASE_S
#undef UART5_BASE_S
#define FPGA_SCC_BASE_S                  0x59300000
#define FPGA_I2S_BASE_S                  0x59301000
#define FPGA_IO_BASE_S                   0x59302000
#define UART0_BASE_S                     0x59303000
#define UART1_BASE_S                     0x59304000
#define UART2_BASE_S                     0x59305000
#define UART3_BASE_S                     0x59306000
#define UART4_BASE_S                     0x59307000
#define UART5_BASE_S                     0x59308000

#endif

#endif
