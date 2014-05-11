/*
* Copyright (c) 2012, Kristoffer Larsen <kri@kri.dk>
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
#ccrl

ccrl is a CC1110 Programer using the cctl-rf bootloader enabling software upgrading via the Radio interface., 
*/


#include "common.h"
#include "app.h"
#include "cons.h"

#include "radio.h"
#include "mac.h"
#include "timer.h"

static __xdata uint8_t cmd[80];
static __xdata uint16_t count = 0;
static uint8_t rf_ack;
static uint8_t cmd_timeout;
__xdata uint8_t rambuf[1024];

uint8_t radio_cmd (void) {
	uint16_t delay_count=0;
	uint8_t retry=10;
	rf_ack=0;
	while (retry && !rf_ack) {
		if (delay_count==0) {
			radio_tx(cmd);
			//cons_puts("Sending cmd\r\n");
			delay_count=10000;
			retry--;
		}
		delay_count--;
		radio_tick();
	}
	if (retry) {
		return 1;
	} else {
		cons_putc(1);
		return 0;
	}
}

void app_init(void)
{
    radio_rx();
}

void app_tick(void)
{
	uint8_t ch,data;
	uint8_t page,seg;
	uint16_t c,d;

	if (cons_getch(&ch)) {
		switch (ch) {
			case 'e':
				while (!cons_getch(&page));
				cmd[0]=4;
				cmd[1]=0xFE;
				cmd[2]=1;
				cmd[3]=page&0x1F;
				cmd[4]=0;
				if (radio_cmd()) goto ack;
				break;

			case 'p':
				while (!cons_getch(&page));
				cmd[0]=4;
				cmd[1]=0xFE;
				cmd[2]=2;
				cmd[3]=page&0x1F;
				cmd[4]=0;
				if (radio_cmd()) goto ack;
				break;

			case 'r':
				while (!cons_getch(&page));
				for (seg=0;seg<16;seg++) {
					cmd[0]=4;
					cmd[1]=0xFE;
					cmd[2]=3;
					cmd[3]=page&0x1F;
					cmd[4]=seg&0x0F;
					if (!radio_cmd()) break;
				}
				for (c=0;c<1024;c++) {
					cons_putc(rambuf[c]);
    					//cons_puthex8(rambuf[c]);
				}
				goto ack;
				break;

			case 'l':
				for (c=0;c<1024;c++) {
					while (!cons_getch(&data));
					rambuf[c]=data;
					//cons_puthex8(data);
				}
				c=0;
				for (seg=0;seg<16;seg++) {
					cmd[0]=68;
					cmd[1]=0xFE;
					cmd[2]=4;
					cmd[3]=0;
					cmd[4]=seg&0x0F;
					for (d=5;d<69;d++) {
						cmd[d]=rambuf[c++];
					}
					if (!radio_cmd()) break;
				}
				goto ack;
				break;
			case 'j':
				cmd[0]=2;
				cmd[1]=0xFE;
				cmd[2]=5;
				if (radio_cmd()) goto ack;
				break;

			case 'b':
				cmd[0]=2;
				cmd[1]=0x20;
				cmd[2]=0xFF;
				radio_tx(cmd);
				//cons_puts("SEND BOOT\r\n");
				goto ack;
				break;

			case 0:
				cmd[0]=4;
				cmd[1]=0xFE;
				cmd[2]=0;
				cmd[3]=0;
				cmd[4]=0;
				radio_cmd();
				break;

ack:
				cons_putc(0);
				//cons_puts("ACK\r\n");
		}
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

void radio_received(__xdata uint8_t *inpkt) {
    uint16_t i;

    if (	inpkt[1]==0xFD && 
		inpkt[2] == cmd[2] &&
		inpkt[2] == cmd[2] &&
		inpkt[2] == cmd[2] ) { 
	    rf_ack=1;
    }
    if (	inpkt[1]==0xFD && 
		inpkt[2] == 3 ) {
	    for (i=0;i<64;i++) 
		    rambuf[i+inpkt[4]*64]=inpkt[i+5];

    } else if (inpkt[1]==0xFD && inpkt[2]==0x10) {
	    cons_putc('W');
    } else {
	    /*
    cons_puthex8(count >> 8);
    cons_puthex8(count & 0xFF);
    cons_puts(": ");
    for (i=0;i<inpkt[0]+1;i++)
        cons_puthex8(inpkt[i]);
    cons_puts("\r\n");
    */
    }
    count++;
}
