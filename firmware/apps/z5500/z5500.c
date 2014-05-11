#include "common.h"
#include "app.h"
#include "cons.h"

#include "radio.h"
#include "mac.h"
#include "timer.h"
#include "watchdog.h"

static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;
static __xdata uint8_t disp_buf[1024];
static __xdata uint8_t disp_string[50];
static uint16_t disp_pos=0;
static uint8_t disp_timeout=0;
static uint8_t vol,input,power;
static uint8_t req_vol,req_input,req_power,req_hdmi;

uint8_t last_rssi;
uint8_t last_lqi;
uint8_t last_src_addr=0;


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

void mark(uint16_t time,uint8_t dev) {
	// Sends an IR mark for the specified number of microseconds.
	// The mark output is modulated at the PWM frequency.
	switch (dev) {
		case 1:
			cbi(P0,4);
			sbi(P0DIR,4);
			break;
		case 2:
			P0_1=1;
			break;
		default:
			break;
	}
	delayMicroseconds(time);
}

void space(uint16_t time,uint8_t dev) {
	// Sends an IR space for the specified number of microseconds.
	// A space is no output, so the PWM output is disabled.
	switch (dev) {
		case 1:
			cbi(P0,4);
			cbi(P0DIR,4);
			break;
		case 2:
			P0_1=0;
			break;
		default:
			break;
	}
	delayMicroseconds(time);
}

void sendNEC(uint32_t data, uint8_t nbits,uint8_t dev) {
	uint8_t i;
	mark(NEC_HDR_MARK,dev);
	space(NEC_HDR_SPACE,dev);
	for (i = 0; i < nbits; i++) {
		if (data & TOPBIT) {
			mark(NEC_BIT_MARK,dev);
			space(NEC_ONE_SPACE,dev);
		} 
		else {
			mark(NEC_BIT_MARK,dev);
			space(NEC_ZERO_SPACE,dev);
		}
		data <<= 1;
	}
	mark(NEC_BIT_MARK,dev);
	space(0,dev);
}

void urx1_isr(void) __interrupt URX1_VECTOR {
	uint8_t c = U1DBUF;  // grab byte from SPI Data Register
	URX1IF = 0;

	// add to buffer if room
	if (disp_pos < sizeof disp_buf)
	{
		disp_buf [disp_pos++] = c;
	}  // end of room available
	sbi(P1,7);
	disp_timeout = 2;
}

void print_disp(void) {
        if (disp_pos && !disp_timeout) {
                uint16_t i;
        	uint8_t char_mode=0;
		uint8_t str_pos=0;
		U1UCR|=U1UCR_FLUSH;
                for (i=0; i<disp_pos; i++) {
                        if (disp_buf[i] == 0x5F) {
                                uint8_t c;
                                c = (disp_buf[i+2]<<4)+disp_buf[i+1];
                                if (char_mode) {
					//if (c >= 0x20 && c< 0x7F ) cons_putc(c);
					//else cons_puthex8(c);
					if (str_pos < sizeof disp_string) {
						disp_string[str_pos++] = c;
					}
                                } else {
                                	//cons_puts("\r\nd"); //Mostly custom char loading.
                                	//cons_puthex8(c);
                                }
                                i++;
                                i++;
                        } else if (disp_buf[i] == 0x1F) {
                                uint8_t c;
                                c = (disp_buf[i+2]<<4)+disp_buf[i+1];
                                //cons_puts("\r\nc");
                                //cons_puthex8(c);
                                if (c&0x80) {
                                        char_mode=1;
					str_pos=c&0x7F;
				} else if (c==0x01) {
                                        char_mode=1;
					for (str_pos=0;str_pos<sizeof disp_string;str_pos++) {
						disp_string[str_pos]=0x20;
					}
					str_pos=0;
                                } else if ((c&0xF8)==8) {
                                        if (c&4) {
                                                cons_puts("Display on\r\n");
						power=1;
                                        } else {
                                                cons_puts("Display off\r\n");
						power=0;
                                        }
                                        char_mode=0;
                                } else {
                                        char_mode=0;
                                }
                                i++;
                                i++;
                                //cons_puts(" p");
                                //cons_puthex8(str_pos);
                        } else {
                                cons_puthex8(disp_buf[i]);
                                cons_puts(" ");
                        }
                }
		if (memcmp(disp_string+7,"Volume",6) == 0) {
			uint8_t i;
			vol=0;
			for (i=20; i < 40 ; i++) {
				if (disp_string[i] == 0x00) vol++;
				if (disp_string[i] == 0x01) vol+=2;
			}

                	cons_puts("Volume Level ");
			cons_puthex8(vol);
			cons_puts("\r\n");
		} else if (memcmp(disp_string,"Input:",6) == 0) {
			switch (disp_string[14]) {
				case 0x81:
					input=1;
					break;
				case 0x82:
					input=2;
					break;
				case 0x83:
					input=3;
					break;
				case 'l':
					input=4;
					break;
				case ' ':
					input=5;
					break;
			}
                	cons_puts("Input: ");
			cons_puthex8(input);
			cons_puts("\r\n");
		} else {
#if defined(CONS_TX_ENABLED)
			uint8_t i;
			for (i=0; i < str_pos ; i++) {
				if (disp_string[i] >= 0x20 && disp_string[i]< 0x7F ) { 
					cons_putc(disp_string[i]);
				} else {
					cons_puthex8(disp_string[i]);
				}
			}
			cons_puts("\r\n");
#endif
		}

                disp_pos = 0;
		cbi(P1,7);
        }  // end of flag set

}


void app_init(void) {
	radio_rx();
	// Display attached to UART1 spi alt2
	P1SEL |= (BIT4 | BIT5 | BIT6 ); 
	P1INP |= (BIT4 | BIT5 | BIT6 ); 
	U1CSR = U1CSR_RE | U1CSR_SLAVE;
	U1GCR = U1GCR_CPOL ;
	PERCFG |= PERCFG_U1CFG;
	URX1IE = 1;

	//XBBO leds
	sbi(P1DIR,7);
	sbi(P0DIR,7);
	cbi(P1,7);
	cbi(P0,7);

	// init vars
	power=1;
	input=0;
	vol=50;
	req_power=0;
	req_input=4;
	req_vol=6;
	req_hdmi=0;

	// Radio
	PKTCTRL1 |= 0x01; // Enable hardware device id chekking
	ADDR = DEV_ID;

	// Optocoupler for HDMI switch
	P0DIR |= BIT1;
	cbi(P0,1);
}

void app_tick(void) {
#if defined(CONS_RX_ENABLED)
	uint8_t ch;

	if (cons_getch(&ch)) {
		pkt[0] = 1;
		pkt[1] = ch;
		//cons_puts("Keypress: "); cons_putc(ch); cons_puts("\r\n");
		//radio_tx(pkt);
		switch (ch) {
			case 't': sendNEC(0x10EF08F7,32,1); break; //Power
			case 'V': sendNEC(0x10EF58A7,32,1); break; //vol+
			case 'v': sendNEC(0x10EF708F,32,1); break; //vol-
			case 'd': req_input=1; break; //direct
			case 'o': req_input=4; break; //optical
			case 'c': req_input=5; break; //coax
			case 'e': sendNEC(0x10EFB847,32,1); break; //Effect
			case 's': sendNEC(0x10EFF807,32,1); break; //Settings
			case 'm': sendNEC(0x10EF6897,32,1); break; //mute
			case 'B': sendNEC(0x10EFC03F,32,1); break; //sub+
			case 'b': sendNEC(0x10EF807F,32,1); break; //sub-
			case 'N': sendNEC(0x10EF40BF,32,1); break; //center+
			case 'n': sendNEC(0x10EF609F,32,1); break; //center-
			case 'X': sendNEC(0x10EF00FF,32,1); break; //surround+
			case 'x': sendNEC(0x10EF20DF,32,1); break; //surround-
			case 'p': req_power=0; break;
			case 'P': req_power=1; break;
			case '1': sendNEC(0x01FE40BF,32,2); break;
			case '2': sendNEC(0x01FE20DF,32,2); break;
			case '3': sendNEC(0x01FEA05F,32,2); break;
			case '4': sendNEC(0x01FE609F,32,2); break;
			case '5': sendNEC(0x01FE10EF,32,2); break;
			case '6': req_vol=6; break;
			case '7': req_vol=12; break;
			case '8': req_vol=18; break;
			case '9': req_vol=38; break;
			case '0': req_vol=0; break;
		}
	}
	/*
    	if (U1CSR & U1CSR_RX_BYTE) {
		cons_putc(U1DBUF);
	}
	*/
#endif
	print_disp();
	if (last_src_addr) {
		static __xdata uint8_t pkt[6];
		pkt[0]=5;
		pkt[1]=last_src_addr;
		pkt[2]=DEV_ID;
		pkt[3]=0x7E;
		pkt[4]=last_rssi;
		pkt[5]=last_lqi;
		timer_delayMS(5);
		last_src_addr=0;
		radio_tx((__xdata uint8_t *) pkt);
	}
}

void app_1hz(void) { 
	if (req_power != power) {
		sendNEC(0x10EF08F7,32,1); //power
	} 
	if (req_power == 1 && power ==1) {
		if (req_input != input) {
			switch (req_input) {
				case 1:
				case 2:
				case 3:
					sendNEC(0x10EF50AF,32,1); //direct
					break; 
				case 4:
					sendNEC(0x10EFD02F,32,1); //optical
					break;
				case 5:
					sendNEC(0x10EF30CF,32,1); //coax
					break;
			}
		}
	}
}

void app_10hz(void) {
	if (req_power == 1 && power ==1 && req_input == input) {
		if ( req_vol > vol) {
			sendNEC(0x10EF58A7,32,1); //vol+
		} else if ( req_vol < vol) {
			sendNEC(0x10EF708F,32,1); //vol-
		}
	}
	if (req_hdmi) {
		switch (req_hdmi) {
			case 1: sendNEC(0x01FE40BF,32,2); break;
			case 2: sendNEC(0x01FE20DF,32,2); break;
			case 3: sendNEC(0x01FEA05F,32,2); break;
			case 4: sendNEC(0x01FE609F,32,2); break;
			case 5: sendNEC(0x01FE10EF,32,2); break;
		}
		req_hdmi=0;
	}


}

void app_100hz(void) { 
	if (disp_timeout) disp_timeout--;
}

void radio_idle_cb(void) {
    radio_rx();
}


void radio_received(__xdata uint8_t *inpkt) {
	uint8_t pktlen;
	uint8_t cmd;
	uint8_t addr;
	uint8_t src_addr;
	uint16_t i;
	pktlen=inpkt[0];
	addr=inpkt[1];
	src_addr=inpkt[2];
	cmd=inpkt[3];
	switch(cmd&0x7F) {
		case 1: // set mode;
			if (inpkt[4] < 2) req_power=inpkt[4];
			if (inpkt[5] < 6) req_input=inpkt[5];
			if (inpkt[6] < 41) req_vol=inpkt[6];
			if (inpkt[7] < 6) req_hdmi=inpkt[7];
			break;
		case 'V': // vol+
			if (req_vol < 40) req_vol++;
			break;
		case 'v': // vol+
			if (req_vol) req_vol--;
			break;
		case 'e': sendNEC(0x10EFB847,32,1); break; //Effect
		case 's': sendNEC(0x10EFF807,32,1); break; //Settings
		case 'm': sendNEC(0x10EF6897,32,1); break; //mute
		case 'B': sendNEC(0x10EFC03F,32,1); break; //sub+
		case 'b': sendNEC(0x10EF807F,32,1); break; //sub-
		case 'N': sendNEC(0x10EF40BF,32,1); break; //center+
		case 'n': sendNEC(0x10EF609F,32,1); break; //center-
		case 'X': sendNEC(0x10EF00FF,32,1); break; //surround+
		case 'x': sendNEC(0x10EF20DF,32,1); break; //surround-
		case 16: // hdmi input
			req_hdmi=inpkt[4];
			break;
		case 0x7E: // Send RF status.
			last_rssi=RSSI;
			last_lqi=LQI&0x7F;
			last_src_addr=inpkt[2];
			break;
		case 0x7F: // reset;
			watchdog_reset();
			break;
	}
	cons_puthex8((count >> 8));
	cons_puthex8((count & 0xFF));
	cons_puts(": ");
	for (i=0;i<inpkt[0]+1;i++)
		cons_puthex8(inpkt[i]);
	cons_puts("\r\n");

	count++;
}

