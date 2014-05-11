/*
* Copyright (c) 2012, Toby Jaffey <toby@sensemote.com>
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
#SniffApp

SniffApp is a simple radio sniffer which dumps received messages to the console. Pressing a key will broadcast a single byte (length byte + 1) radio packet.
*/


#include "common.h"
#include "app.h"
#include "led.h"
#include "cons.h"


#include "radio.h"
#include "mac.h"
#include "timer.h"
#include "pkt.h"
#include "config.h"

static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;

void app_init(void)
{
    radio_rx();
}

void app_tick(void)
{
    uint8_t ch;

    if (cons_getch(&ch))
    {
        pkt[0] = 2;
        pkt[1] = 20;
	pkt[2] = 0xFF;
        cons_puts("Keypress: "); cons_putc(ch); cons_puts("\r\n");
	switch (ch) {
		case '1':
			pkt[2]=1;
			break;
		case '2':
			pkt[2]=2;
			break;
		case '3':
			pkt[2]=3;
			break;
		case '4':
			pkt[2]=4;
			break;
		case 'd':
			cons_puts("U0CSR:");
			cons_puthex8(U0CSR);
			cons_puts(" U0BAUD:");
			cons_puthex8(U0BAUD);
			cons_puts(" U0GCR:");
			cons_puthex8(U0GCR);
			cons_puts(" CLKCON:");
			cons_puthex8(CLKCON);
			cons_puts(" SLEEP:");
			cons_puthex8(SLEEP);
			cons_puts("\r\n");
	}
        radio_tx(pkt);
    }
}

void app_1hz(void)
{
}

void app_10hz(void)
{
}

void app_100hz(void)
{
}

void radio_idle_cb(void)
{
    radio_rx();
}

void radio_received(__xdata uint8_t *inpkt)
{
	uint16_t i;

	cons_puthex8(count >> 8);
	cons_puthex8(count & 0xFF);
	cons_puts(": ");
	for (i=0;i<inpkt[0]+1;i++)
		cons_puthex8(inpkt[i]);
	cons_puthex8(RSSI);
	cons_puthex8(LQI&0x7F);

#ifdef CRYPTO_ENABLED
	if (pkt_dec(inpkt, config_getKeyEnc(), config_getKeyMac())) {
		cons_puts(" Decrypted: ");
		inpkt = PKTPAYLOAD(inpkt);
		for (i=0;i<inpkt[0]+1;i++)
			cons_puthex8(inpkt[i]);
	}
#endif
	cons_puts("\r\n");
	count++;
}


