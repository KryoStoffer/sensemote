#include "common.h"
#include "app.h"
#include "cons.h"

#include "radio.h"
#include "mac.h"
#include "timer.h"
#include "watchdog.h"

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

static __xdata uint8_t pkt[64];
static uint8_t send_status=1;

void app_init(void) {
	radio_rx();
	//XBBO leds
	sbi(P1DIR,7);
	sbi(P0DIR,7);
	cbi(P1,7);
	cbi(P0,7);

	// Radio
	PKTCTRL1 |= 0x02; // Enable hardware device id chekking
	ADDR = 0x30;

	// Change console to 300 baud
#ifdef CRYSTAL_24_MHZ
        U0BAUD = 163;
#endif
#ifdef CRYSTAL_26_MHZ
        U0BAUD = 131;
#endif
	U0GCR = 4;

}


void app_tick(void) {
	uint8_t ch;

	if (cons_getch(&ch)) {
		pkt[0] = 3;
		pkt[1] = 0x30;
		pkt[2] = 5;
		pkt[3] = ch;
		radio_tx(pkt);
	}
	if (send_status) {
		pkt[0] = 3;
		pkt[1] = 0x30;
		pkt[2] = 2;
		pkt[3] = U0GCR;
		radio_tx(pkt);
		send_status=0;
	}
}

void app_1hz(void) { 
}

void app_10hz(void) {
}

void app_100hz(void) { 
}

void radio_idle_cb(void) {
    radio_rx();
}


void radio_received(__xdata uint8_t *inpkt) {
	uint8_t pktlen;
	uint8_t cmd;
	uint8_t addr;
	pktlen=inpkt[0];
	addr=inpkt[1];
	cmd=inpkt[2];
	switch(cmd) {
		case 1: // request data
			//cons_puts("/?!\r\n");
			cons_putc(0x2f);
			cons_putc(0xBf);
			cons_putc(0xA1);
			cons_putc(0x0d);
			cons_putc(0x8a);
			break;
		case 2: // request radio ping
			break;
		case 3: 
			U0GCR++;
			break;
		case 4: 
			U0GCR--;
			break;
		case 255: // reset;
			watchdog_reset();
			break;
	}
}

