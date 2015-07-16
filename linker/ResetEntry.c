/*--------------------------------------------------------------------------
File   : ResetEntry.c

Author : Hoang Nguyen Hoan          Mar. 14, 2014

Desc   : Generic ResetEntry code for ARM CMSIS with GCC compiler

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
#include <string.h>
#include <stdio.h>

extern unsigned long __etext;	// Begin of data in FLASH location
extern unsigned long __data_loc__;
extern unsigned long __data_start__;	// RAM data start
extern unsigned long __data_size__;
extern unsigned long __data_end__;
extern unsigned long __bss_start__;
extern unsigned long __bss_end__;
extern unsigned long __bss_size__;

extern int main (void);
extern void __libc_init_array(void);
extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);

/**
 *	This is entry point after reset
 */
__attribute__ ((section (".AppStart")))
void ResetEntry (void)
{
	/*
	 * Core initialization using CMSIS
	 */
	SystemInit();

	/*
	 * Copy the initialized data of the ".data" segment
	 * from the flash to ram.
	 */
	if (&__data_start__ != &__data_loc__)
		memcpy((void *)&__data_start__, (void *)&__data_loc__, (size_t)&__data_size__);

	/*
	 * Clear the ".bss" segment.
	 */
	memset((void *)&__bss_start__, 0, (size_t)&__bss_size__);

	/*
	 * Call C++ library initialization
	 */
//#ifdef __CPP_SUPPORT
	__libc_init_array();
//#endif

	/*
	 * Now memory has been initialized
	 * Update core clock data
	 */
	SystemCoreClockUpdate();

	/*
	 * We are ready to enter main application
	 */
	main();

	/*
	 * Embedded system don't return to OS.  main() should not mormally
	 * returns.  In case it does, just loop here.
	 */
	while(1);

}

