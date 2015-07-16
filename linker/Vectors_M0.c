/*--------------------------------------------------------------------------
File   : Vector_M0.c

Author : Hoang Nguyen Hoan          Mar. 14, 2014

Desc   : Interrupt Vectors table for ARM Cortex-M0
		 CMSIS & GCC compiler
		 linker section name .Vectors is used for the table

Copyright (c) 2014, I-SYST inc., all rights reserved

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
extern unsigned long __StackTop;
extern void ResetEntry(void);

void DEF_IRQHandler(void) { while(1); }
__attribute__((weak, alias("DEF_IRQHandler"))) void NMI_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void HardFault_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SVC_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void PendSV_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SysTick_Handler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void POWER_CLOCK_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RADIO_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void UART0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SPI0_TWI0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SPI1_TWI1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void GPIOTE_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void ADC_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TIMER2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RTC0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void TEMP_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RNG_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void ECB_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void CCM_AAR_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void WDT_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void RTC1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void QDEC_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void LPCOMP_COMP_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI0_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI1_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI2_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI3_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI4_IRQHandler(void);
__attribute__((weak, alias("DEF_IRQHandler"))) void SWI5_IRQHandler(void);

/**
 * This interrupt vector is by default located in FLASH. Though it can not be
 * changed at runtime. All fcuntions in the vector are weak.  it can be
 * overloaded by application function
 *
 */
__attribute__ ((section(".intvect"), used))
void (* const g_Vectors[])(void) =
{
	/*(void (*) )((int32_t)&__StackTop),*/
	ResetEntry,
	NMI_Handler,
	HardFault_Handler,
	0,
	0,
	0,
	0, 0, 0, 0,
	SVC_Handler,
	0,
	0,
	PendSV_Handler,
	SysTick_Handler,

/* External Interrupts */
    POWER_CLOCK_IRQHandler,		/*POWER_CLOCK */
    RADIO_IRQHandler,		 	/*RADIO */
    UART0_IRQHandler,		 	/*UART0 */
    SPI0_TWI0_IRQHandler,		/*SPI0_TWI0 */
    SPI1_TWI1_IRQHandler,		/*SPI1_TWI1 */
    0,		/*Reserved */
    GPIOTE_IRQHandler,		 	/*GPIOTE */
    ADC_IRQHandler,		 		/*ADC */
    TIMER0_IRQHandler,		 	/*TIMER0 */
    TIMER1_IRQHandler,		 	/*TIMER1 */
    TIMER2_IRQHandler,		 	/*TIMER2 */
    RTC0_IRQHandler,		 	/*RTC0 */
    TEMP_IRQHandler,		 	/*TEMP */
    RNG_IRQHandler,		 		/*RNG */
    ECB_IRQHandler,		 		/*ECB */
    CCM_AAR_IRQHandler,		 	/*CCM_AAR */
    WDT_IRQHandler,		 		/*WDT */
    RTC1_IRQHandler,		 	/*RTC1 */
    QDEC_IRQHandler,		 	/*QDEC */
    LPCOMP_COMP_IRQHandler,		/*LPCOMP_COMP */
    SWI0_IRQHandler,		 	/*SWI0 */
    SWI1_IRQHandler,		 	/*SWI1 */
    SWI2_IRQHandler,		 	/*SWI2 */
    SWI3_IRQHandler,		 	/*SWI3 */
    SWI4_IRQHandler,		 	/*SWI4 */
    SWI5_IRQHandler,		 	/*SWI5 */
    0,		/*Reserved */
    0,		/*Reserved */
    0,		/*Reserved */
    0,		/*Reserved */
    0,		/*Reserved */
    0		/*Reserved */
};

