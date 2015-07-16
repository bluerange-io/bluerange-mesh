/*--------------------------------------------------------------------------
File   : Vectors_M3.c

Author : Hoang Nguyen Hoan          Jan. 25, 2012

Desc   : Interrupt vectors for ARM Cortex-M3

Copyright (c) 2012, I-SYST inc., all rights reserved

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies, and none of the
names : I-SYST or its contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

For info or contributing contact : hnhoan at i-syst dot com

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------------
Modified by          Date              Description

----------------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

extern void ResetEntry(void);

void DEF_IRQHandler(void) { while(1); }

__attribute__((weak, alias("DEF_IRQHandler"))) void NMI_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void HardFault_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void MemManage_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void BusFault_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UsageFault_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SVC_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void DebugMon_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void PendSV_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SysTick_Handler(void);

__attribute__((weak, alias("DEF_IRQHandler"))) void WDT_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER3_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UART0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UART1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UART2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UART3_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void PWM_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void I2C0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void I2C1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void I2C2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SPI_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SSP0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SSP1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void PLL0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RTC_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void EINT0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void EINT1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void EINT2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void EINT3_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void ADC_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void BOD_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void USB_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void CAN_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void DMA_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void I2S_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void ENET_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RIT_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void MCPWM_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void QEI_IRQHandler(void);

/**
 * This interrupt vector is by default located in FLASH. Though it can not be
 * changed at runtime. All fcuntions in the vector are weak.  it can be
 * overloaded by application function
 *
 */
__attribute__ ((section(".intvect"), used))
void (* const g_Vectors[])(void) =
{
	ResetEntry,
	NMI_Handler,
	HardFault_Handler,
	MemManage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	0, 0, 0, 0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,

	WDT_IRQHandler,
	TIMER0_IRQHandler,
	TIMER1_IRQHandler,
	TIMER2_IRQHandler,
	TIMER3_IRQHandler,
	UART0_IRQHandler,
	UART1_IRQHandler,
	UART2_IRQHandler,
	UART3_IRQHandler,
	PWM_IRQHandler,
	I2C0_IRQHandler,
	I2C1_IRQHandler,
	I2C2_IRQHandler,
	SPI_IRQHandler,
	SSP0_IRQHandler,
	SSP1_IRQHandler,
	PLL0_IRQHandler,
	RTC_IRQHandler,
	EINT0_IRQHandler,
	EINT1_IRQHandler,
	EINT2_IRQHandler,
	EINT3_IRQHandler,
	ADC_IRQHandler,
	BOD_IRQHandler,
	USB_IRQHandler,
	CAN_IRQHandler,
	DMA_IRQHandler,
	I2S_IRQHandler,
	ENET_IRQHandler,
	RIT_IRQHandler,
	MCPWM_IRQHandler,
	QEI_IRQHandler
};

