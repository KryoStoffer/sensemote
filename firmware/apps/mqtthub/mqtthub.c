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
#redisHub

hub to use with redis server.

Function 	Ethernet Borad		XINO		cc1110
CLKOUT		1
INT		2			A0		P0_1
WOL		3
MISO		4			D12		P1_7
MOSI		5			D11		P1_6
SCK		6			D13		P1_5
CS		7			D10		P1_4
RESET		8			A1		P0_0
3_3V		9			3_3V		VDD
GND		10			GND		GND

*/

#include "common.h"
#include "app.h"
#include "cons.h"
#include "radio.h"
#include "watchdog.h"
#include "config.h"
#include "tcp.h"
#include "net.h"
#include "pkt.h"

#include "itoa.h"


static __xdata BOOLEAN connected;
static __xdata uint8_t local_seq;

// FIFO of incoming radio packets
#define NUM_CMDPKTS 4
struct cmdpkt
{
    uint8_t eui64[8];
    uint8_t pkt[64];
};
//static __xdata struct cmdpkt cmdpkts[NUM_CMDPKTS];
static __xdata uint8_t cmdpkt_rd;
static __xdata uint8_t cmdpkt_wr;
#define CMDPKT_NEXT(X) ( (X)+1 >= (NUM_CMDPKTS) ? ((X)+1) - (NUM_CMDPKTS) : (X)+1 )
#define CMDPKT_ISEMPTY (cmdpkt_rd == cmdpkt_wr)
#define CMDPKT_ISFULL ( (!CMDPKT_ISEMPTY) && (CMDPKT_NEXT(cmdpkt_wr) == cmdpkt_rd) )

#ifdef CRYPTO_ENABLED
static __xdata uint8_t encpkt[128];
#endif
static uint8_t mqtt_keepalive=0;
static __xdata uint8_t mqtt_rx_buf[128];
static __xdata uint8_t *mqtt_rx_ptr;
static __xdata BOOLEAN send_subscribe;

static int8_t parseHexDigit(uint8_t digit)  // not doing any error checking
{
	    if (digit >= (uint8_t)'0' && digit <= (uint8_t)'9')
		            return (int8_t)digit - '0';
	        return (int8_t)digit + 0xA - 'A';
}

static uint8_t parsehex8(const __xdata char *buf)   // not doing any error checking
{
	    uint8_t r = parseHexDigit(*buf++) << 4;
	        r |= parseHexDigit(*buf);
		    return r;
}


void app_init(void)
{
    connected = FALSE;
    cmdpkt_rd = 0;
    cmdpkt_wr = 0;
    local_seq = 0;
    mqtt_rx_ptr = mqtt_rx_buf;
    send_subscribe = FALSE;
}

void radio_received(__xdata uint8_t *pkt) {
#if 1
    uint8_t i;
    cons_puts("RX: ");
    for (i=0;i<pkt[0]+1;i++)
        cons_puthex8(pkt[i]);
    cons_puts("\r\n");
#endif
}

void radio_idle_cb(void) {
	radio_rx();
}

void app_tick(void)
{
}

void app_10hz(void)
{
}

void app_100hz(void)
{
}

void mqtt_str(__xdata char **buf, char *str) {
	uint8_t c;
	*(*buf)++=0;
	*(*buf)++=strlen(str);
	for (c=0; c<strlen(str);c++) {
		*(*buf)++=str[c];
	}
}

static void mqtt_connect(void) {
	__xdata char *buf = (__xdata char *)tcp_get_txbuf();
	__xdata char *start_buf = (__xdata char *)buf;
	*buf++ = 0x10;  //Connect
	buf++;    //Remainlength
	mqtt_str(&buf,"MQIsdp");
	*buf++ = 0x03;    //Version
	*buf++ = 0x02; 	//Flags
	*buf++ = 0;    //Keep Alive MSB;
	*buf++ = 35;    //Keep Alive LSB;
	mqtt_str(&buf,"CC1110");
	start_buf[1]=buf-start_buf-2;
	tcp_tx(buf-start_buf);
}
static void mqtt_subscribe(void) {
	__xdata char *buf = (__xdata char *)tcp_get_txbuf();
	__xdata char *start_buf = (__xdata char *)buf;
	*buf++ = 0x80;  //Subscribe
	buf++;    //Remainlength
	*buf++ = 0;
	*buf++ = 1;
	mqtt_str(&buf,"/home/rf/+/pkt");
	*buf++ = 0x00;    //qos
	start_buf[1]=buf-start_buf-2;
	tcp_tx(buf-start_buf);
}

void mqtt_pingreq(void) {
	__xdata char *buf = (__xdata char *)tcp_get_txbuf();
	*buf++ = 0xC0;  //Subscribe
	*buf++ = 0;    //Remainlength
	tcp_tx(2);
}

void handle_publish(__xdata uint8_t *pkt, uint8_t len) {
	uint8_t rf_dev;
	(void)len;
	if ((memcmp(pkt+4,"/home/rf/",9)) == 0 && (memcmp(pkt+15,"/pkt",4)==0) ) {
		cons_putsln("Publish Match");
		rf_dev = parsehex8((char *)pkt+13);
        	cons_puthex8(rf_dev);
	}
}

void tcp_rx(__xdata uint8_t *buf, uint16_t len) {
	cons_putsln("tcp_rx called");
    	while(len--) {
        	cons_puthex8(*buf);
		*mqtt_rx_ptr++=*buf++;
		if (!((mqtt_rx_ptr-mqtt_rx_buf == 1) || (mqtt_rx_ptr-mqtt_rx_buf < (mqtt_rx_buf[1]+2)))) {
			cons_putsln(" Packet done");
			switch (mqtt_rx_buf[0] & 0xF0) {
				case 0x20: // CONNACK;
					send_subscribe=TRUE;
					break;
				case 0x30: // PUBLISH;
					handle_publish(mqtt_rx_buf,mqtt_rx_ptr-mqtt_rx_buf);
					break;
				default:
					break;
			}
    			mqtt_rx_ptr=mqtt_rx_buf;
		}
	}
}

void tcp_event(uint8_t event)
{
#if 0
	cons_puts("tcp_event ");
	cons_puthex8(event);
	cons_puts("\r\n");
#endif
	switch(event)
	{
		case TCP_EVENT_RESOLVED:
			if (!connected)
				tcp_connect((char *)config_getHost(), config_getPort());
			break;

		case TCP_EVENT_CONNECTED:
			connected = TRUE;
			mqtt_connect();
			mqtt_keepalive=2;
			break;

		case TCP_EVENT_DISCONNECTED:
			if (connected)
				connected = FALSE;
			break;

		case TCP_EVENT_CANWRITE:
			if (!CMDPKT_ISEMPTY) {
			}
			if (send_subscribe) {
				mqtt_subscribe();
				send_subscribe=FALSE;
			}
			if (!mqtt_keepalive) {
				mqtt_pingreq();
				mqtt_keepalive=30;
			}
			break;
	}
}

void app_1hz(void)
{
    if (!connected && net_isup())
    {
        cons_putsln("RECON");
        tcp_resolv((char *)config_getHost());
    }
    if (mqtt_keepalive) mqtt_keepalive--;
}

