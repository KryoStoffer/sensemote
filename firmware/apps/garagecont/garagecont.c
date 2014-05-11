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
static __xdata uint32_t onetime_key = 0;
static __xdata uint16_t key_timeout = 0;
static __data uint8_t last_direction;
static __data uint8_t key_reply_req =0;
enum {
	UP=0x01,
	DOWN,
	STOP
};


void cmd_open(void);
void cmd_close (void);
void cmd_stop (void);
void send_key (uint8_t dev_id);
uint8_t verify_key (uint32_t rcv_key);
uint8_t last_rssi;
uint8_t last_lqi;
uint8_t last_src_addr;


void app_init(void)
{
	// Setting output for IO port used to trigger garage door
	P0DIR |= BIT0;
	P0_0 =0;

	// Setting input for ports used to detect garage door direction.
	P1DIR =0; // Set input direction
	P1INP = BIT4+BIT5; // disable pull-up/-down
	P1IEN = BIT4+BIT5; // Enable port interupt
	P1SEL =0;
	P1IFG =0;
	PICTL &= ~PICTL_P1ICON;

	IEN2 |= IEN2_P1IE; // Enable port 1 interupt

	/* Enable global interrupts handled by bootloader */
	EA = 1;
	// Radio

	PKTCTRL1 |= 0x01; // Enable hardware device id chekking
	ADDR = DEV_ID;

	cmd_close();
	radio_rx();
}
void app_tick(void)
{
	if (key_reply_req) {
		send_key(key_reply_req);
		key_reply_req=0;
	}
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
	if (key_timeout) key_timeout--;
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
#ifdef CONS_ENABLED
	uint16_t i;
	cons_puthex8(count >> 8);
	cons_puthex8(count & 0xFF);
	count++;
	cons_puts(": ");
	for (i=0;i<inpkt[0]+1;i++)
		cons_puthex8(inpkt[i]);
#endif

#ifdef CRYPTO_ENABLED
	if (pkt_dec(inpkt, config_getKeyEnc(), config_getKeyMac())) {
		inpkt = PKTPAYLOAD(inpkt);
#ifdef CONS_ENABLED
		cons_puts(" decrypted: ");
		for (i=0;i<inpkt[0]+1;i++)
			cons_puthex8(inpkt[i]);
#endif
	} else {
#ifdef CONS_ENABLED
		cons_putsln("Unable to decrupt packet");
#endif
		return;
	}
#endif
#ifdef CONS_ENABLED
	cons_puts("\r\n");
#endif

    switch(inpkt[3]&0x7F)
    {
        case 0x01:  
                if (verify_key((__xdata uint32_t) inpkt[4]))
		       	cmd_open();
        break;
        case 0x02:
                if (verify_key((__xdata uint32_t) inpkt[4]))
                	cmd_close();
        break;
        case 0x03:
                if (verify_key((__xdata uint32_t) inpkt[4]))
                	cmd_stop();
        break;
        case 0x10: // reply new one time key
                timer_delayMS(25);
                key_reply_req=inpkt[2];
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

void port1_isr(void) __interrupt P1INT_VECTOR {
        if (P1IFG&BIT4) {
                cons_puts("up\r\n");
                last_direction=UP;
                P1IFG = ~BIT4;
        } else if (P1IFG&BIT5) {
                cons_puts("down\r\n");
                last_direction=DOWN;
                P1IFG = ~BIT5;
        }
        P1IF =0; // Clear Interupt flag
}
uint8_t send_cmd(void)
{
        uint8_t c = 10;
	uint8_t ret=0;
        P0_0=1;
        if (P1&(BIT4+BIT5)) {
                cons_putsln("Garage in motion, stopping");
                while (c--) {
                        timer_delayMS(100);
                        if (!(P1&(BIT4+BIT5))) {
                                c=0;
				ret=STOP;
                        }
                }
        } else {
                while (c--) {
                        timer_delayMS(100);
                        if (P1 & BIT4) {
                                cons_putsln("Garage Closing");
                                c=0;
				ret=DOWN;
                        }
                        if (P1 & BIT5) {
                                cons_putsln("Garage Opening");
                                c=0;
				ret=UP;
                        }
                }
        }
        P0_0=0;
	return ret;
}

void cmd_stop (void) {
	cons_putsln("cmd_stop");
        if (P1&(BIT4+BIT5)) { // Only send stop if in motion
		cons_putsln("cmd_stop: Sending stop");
		send_cmd();
                timer_delayMS(1000);
	}
}
void cmd_open (void) {
	cons_putsln("cmd_open");
	if (!(P1&BIT5)) {  // Only send something if not in open motion.
		cmd_stop();
		if (send_cmd() == DOWN) {
			cons_putsln("cmd_open: Wrong direction");
			timer_delayMS(1000);
			cmd_stop();
			cons_putsln("cmd_open: try2");
			send_cmd();
		}
	}
}
void cmd_close (void) {
	cons_putsln("cmd_close");
	if (!(P1&BIT4)) {  // Only send something if not in close motion.
		cmd_stop();
		if (send_cmd() == UP) {
			cons_putsln("cmd_close: Wrong direction");
			timer_delayMS(1000);
			cmd_stop();
			cons_putsln("cmd_close: try2");
			send_cmd();
		}
	}
}

void send_key (uint8_t dev_id) {
#ifdef CONS_ENABLED
	uint16_t i;
#endif
	pkt[0] = 7;
	pkt[1] = dev_id;
	pkt[2] = DEV_ID;
	pkt[3] = 0x10;
	random_read(pkt+4,4);
	onetime_key=(uint32_t) pkt[4];
#ifdef CONS_ENABLED
	cons_puts("send_key Sending: ");
	for (i=0;i<pkt[0]+1;i++)
		cons_puthex8(pkt[i]);
#endif
	key_timeout=1800;
	radio_tx((__xdata uint8_t *)pkt);
}

uint8_t verify_key(uint32_t rcv_key) {
	if (!key_timeout) {
		cons_putsln("key not active");
		return 0;
	}
	if (rcv_key == onetime_key) {
		cons_putsln("key match allow command");
		key_timeout=0;
		return 1;
	} else {
		cons_putsln("key not matching deny command");
		return 0;
	}
}
