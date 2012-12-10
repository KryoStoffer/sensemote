#include "common.h"
#include "app.h"
#include "cons.h"

#include "radio.h"
#include "mac.h"
#include "timer.h"
#include "watchdog.h"

static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

#define TOPBIT 0x80000000
#define NEC_HDR_MARK	9000
#define NEC_HDR_SPACE	4500
#define NEC_BIT_MARK	560
#define NEC_ONE_SPACE	1600
#define NEC_ZERO_SPACE	560
#define NEC_RPT_SPACE	2250



void delayMicroseconds(uint32_t time) {
	if (time) {
		T1CCTL2=T1CCTL2_IM | T1CCTL2_MODE; // Enable interupt on compair;
		T1CNTL=0; // Reset counter
		time=((time-250)*75)/100; // Seems that 240us is speend for setting up timer and 
		T1CC2L=time&0xFF; // Set compair value
		T1CC2H=(time>>8)&0xFF; // Set compair value
		T1CTL=T1CTL_DIV_1 | T1CTL_MODE_FREERUN;
		while (!(T1CTL & T1CTL_CH2IF));
		T1CTL=0;
	}
}

void mark(uint16_t time) {
	// Sends an IR mark for the specified number of microseconds.
	// The mark output is modulated at the PWM frequency.
	sbi(P0DIR,4);
	sbi(P0,4);
	delayMicroseconds(time);
}

void space(uint16_t time) {
	// Sends an IR space for the specified number of microseconds.
	// A space is no output, so the PWM output is disabled.
	sbi(P0DIR,4);
	cbi(P0,4);
	delayMicroseconds(time);
}

#define SHARP_BIT_MARK 320
#define SHARP_ONE_SPACE 1680
#define SHARP_ZERO_SPACE 680
#define SHARP_GAP 600000
#define SHARP_TOGGLE_MASK 0x3FF
#define SHARP_RPT_SPACE 3000


void sendSharp(uint32_t data, uint8_t nbits) {
	uint32_t invertdata = data ^ SHARP_TOGGLE_MASK;
	uint8_t i;
	for (i = 0; i < nbits; i++) {
		if (data & 0x4000) {
			mark(SHARP_BIT_MARK);
			space(SHARP_ONE_SPACE);
		} else {
			mark(SHARP_BIT_MARK);
			space(SHARP_ZERO_SPACE);
		}
		data <<= 1;
	}

	mark(SHARP_BIT_MARK);
	space(SHARP_ZERO_SPACE);
	timer_delayMS(40);
	for (i = 0; i < nbits; i++) {
		if (invertdata & 0x4000) {
			mark(SHARP_BIT_MARK);
			space(SHARP_ONE_SPACE);
		} else {
			mark(SHARP_BIT_MARK);
			space(SHARP_ZERO_SPACE);
		}
		invertdata <<= 1;
	}
	mark(SHARP_BIT_MARK);
	space(SHARP_ZERO_SPACE);
	timer_delayMS(40);
}

void sendNEC(uint32_t data, uint8_t nbits) {
	uint8_t i;
	mark(NEC_HDR_MARK);
	space(NEC_HDR_SPACE);
	for (i = 0; i < nbits; i++) {
		if (data & TOPBIT) {
			mark(NEC_BIT_MARK);
			space(NEC_ONE_SPACE);
		} 
		else {
			mark(NEC_BIT_MARK);
			space(NEC_ZERO_SPACE);
		}
		data <<= 1;
	}
	mark(NEC_BIT_MARK);
	space(0);
}

void app_init(void) {
	sbi(P0DIR,4);
	cbi(P0,4);
	radio_rx();
	PKTCTRL1 |= 0x02; // Enable hardware device id chekking
        ADDR = 0x20;

}

void app_tick(void) {
	uint8_t ch;

	if (cons_getch(&ch)) {
		pkt[0] = 1;
		pkt[1] = ch;
		cons_puts("Keypress: "); cons_putc(ch); cons_puts("\r\n");
		//radio_tx(pkt);
		switch (ch) {
			case 'V': sendSharp(0x1AC8,15); break; //vol+
			case 'v': sendSharp(0x18C8,15); break; //vol-
		}
		cons_puts("done\r\n");
	}
}

void app_1hz(void) { }

void app_10hz(void) { }

void app_100hz(void) { }

void radio_idle_cb(void) {
	radio_rx();
}

void radio_received(__xdata uint8_t *inpkt) {
	uint8_t pktlen;
	uint8_t cmd;
	uint8_t addr;
	uint16_t i;
	pktlen=inpkt[0];
	addr=inpkt[1];
	cmd=inpkt[2];

	cons_puthex8(count >> 8);
	cons_puthex8(count & 0xFF);
	cons_puts(": ");
	for (i=0;i<inpkt[0]+1;i++)
		cons_puthex8(inpkt[i]);
	cons_puts("\r\n");

	count++;

	switch(cmd) {
		case 1: // Vol down
			sendSharp(0x18C8,15);
			break;
		case 2: // Vol up
			sendSharp(0x1AC8,15);
			break;
		case 3: // Vol down
			for (i=0;i<inpkt[3];i++)
				sendSharp(0x18C8,15);
			break;
		case 4: // Vol up
			for (i=0;i<inpkt[3];i++)
				sendSharp(0x1AC8,15);
			break;
		case 16: // Phono
			sendSharp(0x1868,15);
			break;
		case 17: // Tuner
			sendSharp(0x1A68,15);
			break;
		case 18: // CD
			sendSharp(0x1968,15);
			break;
                case 255: // Reset
                        watchdog_reset();
                        break;

	}
}
