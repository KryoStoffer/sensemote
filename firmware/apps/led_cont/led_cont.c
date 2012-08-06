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


#include "common.h"
#include "app.h"
#include "ledstrip.h"

#include "radio.h"
#include "timer.h"

#include "config.h"

uint8_t mode, speed;
uint16_t work_color;

void app_pre_init(void) {
	ledstrip_init();
}

void app_init(void) {
	PKTCTRL1 |= 0x02; // Enable hardware device id chekking
	ADDR = DEV_ID;
	mode=0;
	speed=5;
	work_color=Color(0,4,0);

	init_pixels(Color(0,2,0));
}

void app_tick(void) {
}

void app_1hz(void) {
}

void app_10hz(void) {
}

void app_100hz(void) {
	static uint8_t state, state2, countdown;
	if (mode != 0) {
		if (countdown > speed) {
			countdown=0;
			switch(mode) {
				case 1:
					init_pixels(Wheel(state++));
					if (state>95) state=0;
					break;
				case 2:
					setPixel(state,work_color);
					update_led();
					setPixel(state,Color(0,0,0));
					state++;
					if (state>(MAX_LED-1)) state=0;
					break;
				case 3:
					setPixel(state,Wheel(state2++));
					update_led();
					setPixel(state,Color(0,0,0));
					state++;
					if (state>(MAX_LED-1)) state=0;
					if (state2>95) state2=0;
					break;
				case 4:
					setPixel(state,Wheel(state2++));
					update_led();
					state++;
					if (state>(MAX_LED-1)) state=0;
					if (state2>95) state2=0;
					break;
				case 254:
					init_pixels(Color(31,0,0));
					update_led();
					break;
				default:
					break;
			}
		} else {
			countdown++;
		}
	}
}

void radio_idle_cb(void) {
	radio_rx();
}

void radio_received(__xdata uint8_t *inpkt) {
	uint8_t pktlen;
	uint8_t cmd;
	uint8_t addr;
	uint8_t a,b,c;
	pktlen=inpkt[0];
	addr=inpkt[1];
	cmd=inpkt[2];
	switch(cmd) {
		case 1: // ALL_LED_COLOR
			init_pixels(Wheel(inpkt[3]));
			break;
                case 2: // ALL_LED_RGB:
                        init_pixels(Color(inpkt[3],inpkt[4],inpkt[5]));
                        break;
                case 3: // LED_RGB:
                        setPixel(inpkt[3], Color(inpkt[4],inpkt[5],inpkt[6]));
                        update_led();
                        break;
                case 4: // COLOR:
                        work_color=Color(inpkt[3],inpkt[4],inpkt[5]);
                        break;
                case 5: // MAP_COLOR:
                        a=0;
                        for (b=0; b < (pktlen-2) ; b++) {
                                c=0x80;
                                while (c) {
                                        if (c & inpkt[b+3]) {
                                                setPixel(a,work_color);
                                        }
                                        c>>=1;
                                        a++;
                                }
                        }
                        update_led();
                        break;
                case 6: // MAP_COLOR_CLEAR:
                        a=0;
                        for (b=0; b < (pktlen-2) ; b++) {
                                c=0x80;
                                while (c) {
                                        if (c & inpkt[b+3]) {
                                                setPixel(a,work_color);
                                        } else {
                                                setPixel(a,0);
                                        }
                                        c>>=1;
                                        a++;
                                }
                        }
                        update_led();
                        break;
		case 7: // Mode and speed
			mode=inpkt[3];
			speed=inpkt[4];
			break;
		default:
			init_pixels(0x0000);
			break;
	}
}
