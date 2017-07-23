/* -*- mode: c++; tab-width: 4; c-basic-offset: 4 -*- */

/* Written by Borge Strand-Bergesen 20150701
 * Interaction with WM8805
 *
 * Copyright (C) Borge Strand-Bergesen, borge@henryaudio.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


/*
Using the WM8805 as an SPDIF receiver in software control mode

Here is an engineer's viewpoint and experience on programming the WM8805. I've decided
to share how I got this chip to work well on all relevant audio sample rates (including
176.4 and 192ksps). Hopefully, this will be a good complement to the datasheet. You may
ask why I put this together. Basically, it was harder than the toughest suduko I've
ever solved, and it felt much more meaningful. I may put the technology into a product
one day in the future, but until then it's a very nice addition to my design toolbox.

The code can be found at:
https://github.com/borgestrand/sdr-widget/blob/audio-widget-experimental/src/wm8805.c

The WM8805 from Wolfson / Cirrus is a multi-port SPDIF receiver and SPDIF transmitter
for consumer mode signaling. (I.e. no AES/EBU.) Its main feature is a digital fractional
PLL for clock recovery. Most other SPDIF receivers use an analog PLL structure which
depends on external passive components. Proper listening tests will come next.

This work on programming the WM8805 was is add-on to the Audio Widget project. Just like
the Audio Widget project, the commercially available Henry Audio USB DAC 128 mkII and the
SDR-Widget project, source code is open source.

The WM8805 and the WM8804 are difficult to use. But the good news is that the code base
here makes the WM8805 work very well with two different SPDIF sources and all sample
rates. I have described some shortcomings of the chip, but to a great extent they can be
overcome. The code handles automatic detection and re-configuration of the notorious
176.4ksps sample rate along with 44.1, 48, 88.2, 96 and 192ksps. Another piece of good
news is that the I2C / two-wire control worked right out of the box with the routines in
the AVR32 code base. That's a first for me. I'm a bit too used to bit-banging poorly
implemented I2C....

The hardware has been tested with SPDIF and TOSLINK inputs up to 192ksps. The hardware
is essentially a modified Henry Audio USB DAC 128 mkII design where a few IO lines
have been reassigned. For an experienced electrical engineer, detailed knowledge of the
hardware platform is probably not needed in order to port this design.

The devil is in the details. The sample rate detector of the WM8805 uses the same status
information to encode 44.1 and 48ksps, the same for 88.2 and 96, and the same for 176.4
and 192. In itself this isn't that bad, if it wasn't for the fact that 192ksps needs a
different initial PLL setting from the other five. So there's basically no way to
determine from the chip itself whether that setting is needed. The sample rate detector
inside the WM8805 can't be trusted, and neither can its sample rate change interrupt.
(Yes, I did try with an averaged one-size-fits-all PLL setting. That ended up not
working for any sample rate...)

Luckily, the WM8805 sends out an I2S word clock of the correct sample rate, even though
the PLL isn't correctly set up. This means the external sample rate detector can be used
to determine which PLL setting to apply. The WM8805 interrupt is polled in order to
determine whether it has lost lock.

The pivotal part of the code is the external sample rate detector. It was programmed in
AVR32 assembly code. (Yes, I cheated and looked at the .asm which some sample .c compiled
into. With that as a starting point and a lot of time having coded other assembly languages
it was easy hacking even without knowledge of this particular machine language.) The delays
were verified with a square wave generator and later with actual I2S. Change the MCU
frequency and all the delays must be redone.

The FreeRTOS scheduler of the Audio Widget project is halted for a bit more than one
audio word clock period, during which time a counter is decremented. If you wish to
port this effort to a different platform, the first thing to do is determine if have
the resources available to implement a good sample rate detector. Then you must
understand the function wm8805_srd(). The .c -> .asm recipe is in its comments.

Because the starting point for the design is an USB DAC, there is code in place to make
the WM8805 and the MCU based USB audio receiver share one I2S interface to the DAC. The
RTOS software uses a semaphore to determine which task (WM8805 monitor or USB audio) may
control the I2S and MCLK multiplexers. If your goal is to only use the WM8805 part of
the design, the semaphore code can be easily designed out.

Another shortcoming of the WM8805 is the lack of a user controlled mute function. I'd like
it to generate some functional and muted I2S when it's trying to obtain lock or doing
other things without valid digital audio arriving. There is no soft mute or anti-pop
functionality which one can pretty much expect in modern digital audio. Instead the code
switches in the USB receiver's muted I2S output whenever a WM8805 mute is needed. The USB
receiver's output buffer is often cleared in the USB code. That means there are few
explicit calls to wm8805_mute(). And those that are there can probably be removed.

The current implementation of wm8805_mute() is crude. At least it should wait until a
sample boundary before setting the I2S data line to 0. That is because a small negative
value truncated with zeros will become a large negative value to the DAC. With no control
of the exact time when the USB's I2S output replaces that of the WM8805, this condition is
hard to prevent, and some pops may be expected. A modest hardware fix would be to have an
AND gate zero the I2S data line, and have its control signal be latched by a negative
transition of the word clock. A fuller software fix would be to route the WM8805's I2S
output into the MCU's ADC interface so that gentle muting and I2S multiplexing can be done
in software. My prototype hardware is prepared for this option but I haven't yet begun
building it.

The WM8805 in this design supports two SPDIF inputs (one over a TOSLINK plug). When USB
audio is playing the WM8805 is shut down. But when that is not the case, the WM8805 scans
its inputs until it finds one with a lock. With lock it sends that input to the DAC's I2S
input. The WM8805 spends 200ms or so trying to lock to one input before it gives up and
tries the next. This can easily be extended to more than two SPDIF inputs into the WM8805.
This automatic input selector is working well. The only flipside is that it doesn't respond
immediately to digital audio appearing on an input. If that is desired, the number of inputs
must be reduced, or the user must press a button for channel selection.

While the scan mode is quick, the code takes more time to give up on a WM8805 or USB when
there has been actual audio coming from that interface. Only when the interface looses
lock or becomes muted for more than 2s does the code start looking at other inputs for
valid audio.

This mechanism also extends to the USB interface. With input_select == MOBO_SRC_UAC2 or
MOBO_SRC_UAC1 the WM8805 is powered down and the USB receiver owns the DAC's I2S interface.
When the USB audio is muted or halted, input_select is set to MOBO_SRC_NONE and the WM8805
is powered up to start scanning its inputs. When it finds a valid one, input_select is set
to MOBO_SRC_SPDIF or MOBO_SRC_TOS2, and the I2S interface is switched over to the WM8805.
And when the WM8805 is muted or looses link, the I2S is once again handed over to the USB
system. At first, with input_select == MOBO_SRC_NONE, this is done to mute the DAC.

If this project is of interest to you, please let me know! I hope to see you at either:
 Audio Widget mailing list: audio-widget@googlegroups.com
 https://www.facebook.com/henryaudio
 borge@henryaudio.com

*/

#include "stdio.h"
#include "wm8805.h"
#include "I2C_wrapper.h"
#include "mbed.h"

// Global status variable
volatile wm8805_status_t wm8805_status = {0, 1, 0, 0, FREQ_TIMEOUT, WM8805_PLL_NONE, 1};

//!
//! @brief Polling routine for WM8805 hardware
//!
void wm8805_poll(void) {

}

DigitalOut wm8805_reset_pin(p8);

void wm8805_reset(void) {
	 wait(1);
	 wm8805_reset_pin = 0;
	 wait(0.2);
	 wm8805_reset_pin = 1;
	 wait(1);
}

// Start up the WM8805
void wm8805_init(void) {
	I2C_write_byte(WM8805_DEV_ADDR, 0x00, 0x01);  // Reset
	I2C_write_byte(WM8805_DEV_ADDR, 0x03, 0x21);	// PLL_K[7:0] 21
	I2C_write_byte(WM8805_DEV_ADDR, 0x04, 0xFD);	// PLL_K[15:8] FD
	I2C_write_byte(WM8805_DEV_ADDR, 0x05, 0x36);	// 7:0 , 6:0, 5-0:PLL_K[21:16] 36
	I2C_write_byte(WM8805_DEV_ADDR, 0x06, 0x07);	// 7:0 , 6:0 , 5:0 , 4:0 Prescale/1 , 3-2:PLL_N[3:0] 7
  I2C_write_byte(WM8805_DEV_ADDR, 0x07, 0x16);	// 7:0 , 6:0, 5-4:MCLK=128fs , 3:1 MCLKDIV=1 , 2:1 FRACEN , 1-0:0
	I2C_write_byte(WM8805_DEV_ADDR, 0x08, 0x18);	// **** changed from github 7:0 CLK2, 6:1 auto error handling disable, 5:1 zeros@error, 4:1 CLKOUT enable, 3:0 CLK1 out, 2-0:000 RX0
  I2C_write_byte(WM8805_DEV_ADDR, 0x09, 0x00);  // **** changed from default CMOS input vs. 500mv comparator
  I2C_write_byte(WM8805_DEV_ADDR, 0x0A, 0b11100100);	// REC_FREQ:mask (broken in wm!), DEEMPH:ignored, CPY:ignored, NON_AUDIO:active
	I2C_write_byte(WM8805_DEV_ADDR, 0x17, 0x00);	// 7:4 GPO1=INT_N (=SPIO_00, PX54), 3:0 GPO0=INT_N, that pin has 10kpull-down
	I2C_write_byte(WM8805_DEV_ADDR, 0x18, 0xF7);	// 7:4 GPO3='0', 3:0 GPO2=UNLOCK, WM8805_CSB_PIN=PX37 on HW_GEN_DIN20. OK with initial software set to 1?
	I2C_write_byte(WM8805_DEV_ADDR, 0x1A, 0xCF);	// 7:4 GPO7=ZEROFLAG (=SPIO_04, PX15), 3:0 GPO6=0, that pin is grounded SPDIF in via write to 0x1D:5
	I2C_write_byte(WM8805_DEV_ADDR, 0x1C, 0xCE);	// 7:1 I2S alive, 6:1 master, 5:0 normal pol, 4:0 normal, 3-2:11 or 10 24 bit, 1-0:10 I2S ? CE or CA ?
	I2C_write_byte(WM8805_DEV_ADDR, 0x1D, 0xC8);	// 7 SPD_192K_EN = 1, Change 6:1, disable data truncation, run on 24 bit I2S

	I2C_read_byte(WM8805_DEV_ADDR, 0x0B);			// Clear interrupts
	I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x00);	// Power down 7-6:0, 5:0 OUT, 4:1 _IF, 3:1 _OSC, 2:0 TX, 1:1 _RX, 0:1 _PLL,
}

// Turn off wm8805, why can't we just run init again?
void wm8805_sleep(void) {
	I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x1F);	// Power down 7-6:0, 5:0 OUT, 4:1 _IF, 3:1 _OSC, 2:1 _TX, 1:1 _RX, 0:1 _PLL,
}

// Select input channel of the WM8805
void wm8805_input(uint8_t input_sel) {
  //power control
	I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x06);		// 7-6:0, 5:0 OUT, 4:0 IF, 3:0 OSC, 2:1 _TX, 1:1 _RX, 0:0 PLL,
	I2C_write_byte(WM8805_DEV_ADDR, 0x08, 0x30 + input_sel);	// 7:0 CLK2, 6:0 auto error handling enable, 5:1 zeros@error, 4:1 CLKOUT enable, 3:0 CLK1 out, 2-0:4 RX4
	I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x04);		// 7-6:0, 5:0 OUT, 4:0 IF, 3:0 OSC, 2:1 _TX, 1:0 RX, 0:0 PLL,
}

// Select PLL setting of the WM8805
void wm8805_pll(void) {
	uint8_t pll_sel = WM8805_PLL_NONE;

	wm8805_status.frequency = wm8805_srd();
	if (  ( (wm8805_status.frequency == FREQ_192) && (wm8805_status.pllmode != WM8805_PLL_192) ) ||
		    (  (wm8805_status.frequency != FREQ_192) && (wm8805_status.pllmode != WM8805_PLL_NORMAL) ) ) {
		if (wm8805_status.frequency == FREQ_192)
			wm8805_status.pllmode = WM8805_PLL_192;
		else
			wm8805_status.pllmode = WM8805_PLL_NORMAL;

		pll_sel = wm8805_status.pllmode;
	}

	// NO need for new PLL setup
	if (pll_sel == WM8805_PLL_NONE)
		return;

	// PLL setup has changed
	//I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x06);		// 7-6:0, 5:0 OUT, 4:0 IF, 3:0 OSC, 2:1 _TX, 1:1 _RX, 0:0 PLL,

	// Default PLL setup for 44.1, 48, 88.2, 96, 176.4
	if (pll_sel == WM8805_PLL_NORMAL) {
		printf("Setting WM8805 PLL_NORMAL\n\r");
		I2C_write_byte(WM8805_DEV_ADDR, 0x03, 0x21);	// PLL_K[7:0] 21
		I2C_write_byte(WM8805_DEV_ADDR, 0x04, 0xFD);	// PLL_K[15:8] FD
		I2C_write_byte(WM8805_DEV_ADDR, 0x05, 0x36);	// 7:0 , 6:0, 5-0:PLL_K[21:16] 36
		I2C_write_byte(WM8805_DEV_ADDR, 0x06, 0x07);	// 7:0 , 6:0 , 5:0 , 4:0 Prescale/1 , 3-2:PLL_N[3:0] 7
	}

	// Special PLL setup for 192
	else if (pll_sel == WM8805_PLL_192) {	// PLL setting 8.192
		printf("Setting WM8805 PLL_192\n\r");
		I2C_write_byte(WM8805_DEV_ADDR, 0x03, 0xBA);	// PLL_K[7:0] BA
		I2C_write_byte(WM8805_DEV_ADDR, 0x04, 0x49);	// PLL_K[15:8] 49
		I2C_write_byte(WM8805_DEV_ADDR, 0x05, 0x0C);	// 7:0,  6:0, 5-0:PLL_K[21:16] 0C
		I2C_write_byte(WM8805_DEV_ADDR, 0x06, 0x08);	// 7: , 6: , 5: , 4: , 3-2:PLL_N[3:0] 8
	}

	I2C_write_byte(WM8805_DEV_ADDR, 0x1E, 0x00);		// 7-6:0, 5:0 OUT, 4:0 IF, 3:0 OSC, 2:1 _TX, 1:0 RX, 0:0 PLL,
}

// Set up WM8805 CLKOUTDIV so that CLKOUT is in the 22-24MHz range
void wm8805_clkdiv(void) {
	uint8_t temp;
	temp = I2C_read_byte(WM8805_DEV_ADDR, 0x0C);		// Read SPDSTAT
	temp = temp & 0x30;					      // Consider bits 5-4

	if ( (temp == 0x20) || (temp == 0x30) )	// 44.1, 48, or 32
		I2C_write_byte(WM8805_DEV_ADDR, 0x07, 0x0C);	// 7:0 , 6:0, 5-4:MCLK=512fs , 3:1 MCLKDIV=1 , 2:1 FRACEN , 1-0:0
	else if (temp == 0x10)				// 88.2 or 96
		I2C_write_byte(WM8805_DEV_ADDR, 0x07, 0x1C);	// 7:0 , 6:0, 5-4:MCLK=256fs , 3:1 MCLKDIV=1 , 2:1 FRACEN , 1-0:0
	else								          // 176.4 or 192
		I2C_write_byte(WM8805_DEV_ADDR, 0x07, 0x2C);	// 7:0 , 6:0, 5-4:MCLK=128fs , 3:1 MCLKDIV=1 , 2:1 FRACEN , 1-0:0
}

uint32_t wm8805_srd(void) {
	uint32_t freq = FREQ_44;
	uint32_t freq_prev;
	int i;

	i = 0;
	freq_prev = FREQ_INVALID;

  //put code here to detect sample rate

	return freq;
}
