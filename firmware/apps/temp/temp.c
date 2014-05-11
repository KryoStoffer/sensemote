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
#include "adc.h"
#include "sleep.h"


static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;
static uint8_t temp_timer = 10;
uint8_t last_rssi;
uint8_t last_lqi;
uint8_t last_src_addr=0;
uint8_t send_temp=1;


void app_init(void)
{
	PKTCTRL1 |= 0x01; // Enable hardware device id chekking
	ADDR = DEV_ID;

//	radio_rx();

	P0DIR |= 2;
	P0 |= 2;
	P0INP|=1;

	P1DIR |= 3;
	P1 = 3;
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
	if (send_temp) {
		uint32_t temp;
		static uint16_t volt;

		static __xdata uint8_t pkt[8];

		send_temp=0;

		sleep_powersave(20);

		P1=3; // Turn on led.
		P0=2; // Turn on pull up for voltage devider.
		temp=adc_read(0,TRUE);

/*
 * Final set of parameters            Asymptotic Standard Error
 * =======================            ==========================
 * 
 * A0              = 1292.37          +/- 1.193        (0.09232%)
 * tau             = 0.0854718        +/- 0.0001726    (0.202%)
 */
		temp*=100;
		temp=129237UL-temp;
		temp*=8547UL;
		temp/=100000UL;

		pkt[0]=7;
		pkt[1]=0xF0;
		pkt[2]=DEV_ID;
		pkt[3]=0x01;
		pkt[4]=(temp>>8);
		pkt[5]=(temp&0xFF);
		pkt[6]=(volt>>8);
		pkt[7]=(volt&0xFF);
		radio_tx((__xdata uint8_t *) pkt);
		volt=adc_readBattery();
		temp=adc_read(0,TRUE);
		temp_timer=10;
		P0=0;
	}
}

void app_1hz(void)
{
	//if (temp_timer) temp_timer--;
}

void app_10hz(void)
{
}

void app_100hz(void)
{
}

void radio_idle_cb(void)
{
    //radio_rx();
    P1=0; // Led off
    send_temp=1;
}

void radio_received(__xdata uint8_t *inpkt)
{
    switch(inpkt[3]&0x7F)
    {
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

