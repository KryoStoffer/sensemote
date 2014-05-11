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


static __xdata uint8_t pkt[64];
static __xdata uint16_t count = 0;
static uint8_t cmd_timeout = 0;
uint8_t last_rssi;
uint8_t last_lqi;
uint8_t last_src_addr=0;

enum {
	UP=0x01,
	DOWN,
	STOP
};


void app_init(void)
{
	// Setting output for IO port used to trigger garage door
	P1 = 0;
	P1DIR |= (BIT4 | BIT5 | BIT6 | BIT7);

	PKTCTRL1 |= 0x01; // Enable hardware device id chekking
	ADDR = DEV_ID;

	radio_rx();
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
	if (cmd_timeout) {
		cmd_timeout--;
	} else {
		P1=0;
	}
}

void radio_idle_cb(void)
{
    radio_rx();
}

void radio_received(__xdata uint8_t *inpkt)
{

#ifdef CRYPTO_ENABLED
	if (pkt_dec(inpkt, config_getKeyEnc(), config_getKeyMac())) {
		inpkt = PKTPAYLOAD(inpkt);
	} else {
		return;
	}
#endif

    switch(inpkt[3]&0x7F)
    {
        case 0x01:  
		P1_4=1;
        break;
        case 0x02:
		P1_5=1;
        break;
        case 0x03:
		P1_6=1;
        break;
        case 0x04:
		P1_7=1;
        break;
        case 0x11:  
		P1_4=0;
        break;
        case 0x12:
		P1_5=0;
        break;
        case 0x13:
		P1_6=0;
        break;
        case 0x14:
		P1_7=0;
        break;
        case 0x20: // momentary up
		cmd_timeout=25;
		P1=(BIT4|BIT6);
        break;
        case 0x21: // momentary down
		cmd_timeout=25;
		P1=(BIT5|BIT7);
        break;
        case 0x22: // stop
		P1=0;
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

