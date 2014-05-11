#include "common.h"
#include "app.h"
#include "led.h"
#include "cons.h"


#include "radio.h"
#include "mac.h"
#include "timer.h"
#include "pkt.h"
#include "config.h"
#include "watchdog.h"
#include "random.h"
#include "pwm.h"


static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;
static uint8_t cmd_timeout = 0;
uint8_t last_rssi;
uint8_t last_lqi;
uint8_t last_src_addr=0;


void app_init(void)
{
	PKTCTRL1 |= 0x01; // Enable hardware device id chekking
	ADDR = DEV_ID;

	radio_rx();

	//P0SEL=0x00;
	//P1SEL=0x00;
	//P0DIR=0xFF;
	//P1DIR=0xFF;


	pwm_p1_4_init();
	pwm_p1_3_init();
	pwm_p2_0_init();
	pwm_p2_3_init();
	//T1 Alt 1 Ch0 = P0.2 Ch1 = P0.3 Ch3=P0.4
	//P0SEL |= (1<<2) | (1<<3) | (1<<4);
	//P0DIR |= (1<<2) | (1<<3) | (1<<4);
	//P2DIR |= (0xC0); // Timer1 has priority over USART.
	//T1CCTL0 = T1C0_SET_CMP_UP_CLR_0 | T1CCTL0_MODE; // mode
	//T1CCTL1 = T1C1_SET_CMP_UP_CLR_0 | T1CCTL1_MODE; // mode
	//T1CCTL2 = T1C2_SET_CMP_UP_CLR_0 | T1CCTL2_MODE; // mode
	//T1CC0L = 0xFF;
	//T1CC0H = 0xFF;  
	//T1CC1L = 0x00;
	//T1CC1H = 0x80;  
	//T1CC2L = 0x00;
	//T1CC2H = 0x80;  
	//T1CTL = T1CTL_DIV_1 | T1CTL_MODE_FREERUN;


/*
	// T3, Alt1, Ch1 = P1.4
	PERCFG = (PERCFG & ~PERCFG_T3CFG);   // Timer3 alternate 1 (P1.4)
	P1SEL |= (1<<4);        // P1.4 non-GPIO function
	P1DIR |= (1<<4);
	P2SEL |= (1<<5);    // t3 has priority over uart
	T3CCTL1 = T3C1_SET_CMP_UP_CLR_0 | T3CCTL1_MODE; // mode
	T3CC0 = 0xFF;   // period
	T3CC1 = 0x00;   // duty
	T3CTL = T3CTL_START | T3CTL_CLR | T3CTL_MODE_MODULO;


	// T4, Alt2, Ch1 = P2.3
	PERCFG = (PERCFG & ~PERCFG_T4CFG) | PERCFG_T4CFG;   // Timer4 alternate 2 (P2.3)
	P2SEL |= P2SEL_SELP2_3;        // P2.3 non-GPIO function
	T4CCTL1 = T4CCTL1_SET_CMP_UP_CLR_0 | T4CCTL1_MODE; // mode
	T4CC0 = 0xFF;   // period
	T4CC1 = 0x00;   // duty
	T4CTL = T4CTL_START | T4CTL_CLR | T4CTL_MODE_MODULO;
	*/
}

void app_tick(void)
{
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
	switch(inpkt[3]&0x7F) {
		case 0x01:  
			pwm_p1_4_set(0xFF-inpkt[4]);
			break;
		case 0x02:  
			pwm_p1_4_set(0xFF);
			break;
		case 0x03:  
			pwm_p1_4_set(0x00);
			break;
		case 0x11:  
			pwm_p1_3_set(0xFF-inpkt[4]);
			break;
		case 0x12:  
			pwm_p1_3_set(0xFF);
			break;
		case 0x13:  
			pwm_p1_3_set(0x00);
			break;
		case 0x21:  
			pwm_p2_0_set(0xFF-inpkt[4]);
			break;
		case 0x22:  
			pwm_p2_0_set(0xFF);
			break;
		case 0x23:  
			pwm_p2_0_set(0x00);
			break;
		case 0x31:  
			pwm_p2_3_set(0xFF-inpkt[4]);
			break;
		case 0x32:  
			pwm_p2_3_set(0xFF);
			break;
		case 0x33:  
			pwm_p2_3_set(0x00);
			break;
		case 0x7E: // Send RF status.
			last_rssi=RSSI;
			last_lqi=LQI&0x7F;
			last_src_addr=inpkt[2];
			break;
		case 0x7F:
			watchdog_reset();
			break;
	}
}

