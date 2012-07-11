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

#include "json.h"
#include "line.h"
#include "itoa.h"

#include "pbrf.h"

static __xdata BOOLEAN connected;
static __xdata uint8_t local_seq;

// FIFO of incoming radio packets
#define NUM_CMDPKTS 4
struct cmdpkt
{
    uint8_t eui64[8];
    uint8_t pkt[64];
};
static __xdata struct cmdpkt cmdpkts[NUM_CMDPKTS];
static __xdata uint8_t cmdpkt_rd;
static __xdata uint8_t cmdpkt_wr;
#define CMDPKT_NEXT(X) ( (X)+1 >= (NUM_CMDPKTS) ? ((X)+1) - (NUM_CMDPKTS) : (X)+1 )
#define CMDPKT_ISEMPTY (cmdpkt_rd == cmdpkt_wr)
#define CMDPKT_ISFULL ( (!CMDPKT_ISEMPTY) && (CMDPKT_NEXT(cmdpkt_wr) == cmdpkt_rd) )

enum {REDIS_METHOD_GET, REDIS_METHOD_SET, REDIS_METHOD_SUB, REDIS_METHOD_UNSUB};    // enum must match str table
static const char *method_strs[] = {"get", "set", "subscribe", "unsubscribe"};

#define redis_get(key, tok)            redis_request(REDIS_METHOD_GET, key, NULL, tok)
#define redis_set(key, val, tok)       redis_request(REDIS_METHOD_SET, key, val, tok)
#define redis_subscribe(key, tok)      redis_request(REDIS_METHOD_SUB, key, NULL, tok)
#define redis_unsubscribe(key, tok)    redis_request(REDIS_METHOD_UNSUB, key, NULL, tok)

#ifdef CRYPTO_ENABLED
static __xdata uint8_t encpkt[128];
#endif

static void redis_request(uint8_t method, const char *key, const char *val, const char *token) {
	// FIXME, no bounds checking done
	__xdata char *buf = (__xdata char *)tcp_get_txbuf();
	(void)val;
	(void)token;
	buf[0] = 0;

	strcat(buf, method_strs[method]);
	strcat(buf, " ");

	strcat(buf, key);
	if (method == REDIS_METHOD_SET)
	{
		strcat(buf, " ");
		strcat(buf, val);
	}
	strcat(buf, "\r\n");
	//
	//cons_puts("<-");
	//cons_puts(buf);
	tcp_tx(strlen(buf));
}

void app_init(void)
{
    connected = FALSE;
    cmdpkt_rd = 0;
    cmdpkt_wr = 0;
    local_seq = 0;

}

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

static char *str_puthex8(char *buf, uint8_t x)   // write two chars, no \0
{
    *buf++ = nibble_to_char((x & 0xF0) >> 4);
    *buf++ = nibble_to_char(x & 0x0F);
    return buf;
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

void line_rx(const __xdata char *line) {
	static __xdata char hex[48+1];
	static __xdata char buf[64]; 
	static __xdata uint8_t addr;
	uint8_t vlen;
	uint8_t i;

	cons_puts("->");
	cons_putsln(line);

	json_getstr(line, "\"addr\"", buf, sizeof(buf));
	if (strlen(buf) == 2) {
		addr = parsehex8(buf);
	} else {
		return;
	}

	json_getstr(line, "\"hex\"", hex, sizeof(hex));

	vlen = strlen(hex);
	/* sanity check if length is even */
	if (vlen & 1 )
		return;

	buf[0] = 1 + (vlen>>1);
	buf[1]=addr;
	//cons_putdec(buf[0]);
	for (i=0;i<vlen;i+=2) {
		buf[1+1+(i>>1)] = parsehex8(hex+i);
		//cons_puthex8(buf[1+1+(i>>1)]);
		//cons_putsln("parsed");
	}
	//cons_dump(buf,buf[0]+1);

	radio_tx((__xdata uint8_t *)buf);

}

void tcp_rx(__xdata uint8_t *buf, uint16_t len)
{
    while(len--)
        line_putc(*buf++);
}

static void enctoken(char *buf, const __xdata uint8_t *eui64, uint8_t cmd, uint8_t seq)
{
    uint8_t i;
    for (i=0;i<8;i++)
        buf = str_puthex8(buf, eui64[i]);
    buf = str_puthex8(buf, cmd);
    buf = str_puthex8(buf, seq);
    *buf = 0;
}


static void handle_pkt(const __xdata uint8_t *eui64, const __xdata uint8_t *inpkt)
{
    const __xdata char *key;
    const __xdata char *val;
    char tok[16+2+2+1];  // 0x eui64 + cmd + seq + \0
    uint8_t cmd;
    uint8_t seq;

    if (inpkt[0] < 4)   // cmd + seq + \0 + \0
        return;

    cmd = inpkt[1];
    seq = inpkt[2];
    key = (const __xdata char *)(inpkt+3);
    val = key;
    while(*val != 0 && val < (const __xdata char *)(inpkt + (inpkt[0]+1)))
        val++;
    if (*val == 0)  // found key's terminator 
        val++;  // advance to beginning of val

#if 0
    cons_puts("pkt c=");
    cons_puthex8(cmd);
    cons_puts(" s=");
    cons_puthex8(seq);
    cons_puts(" k=");
    cons_puts(key);
    cons_puts(" v=");
    cons_puts(val);
    cons_puts(" e=");
    for (i=0;i<8;i++)
        cons_puthex8(eui64[i]);
    cons_puts("\r\n");
#endif

    switch(cmd)
    {
        case RF_CMD_GET:
            enctoken(tok, eui64, RF_CMD_INF, seq);
            redis_get(key, tok);
        break;
        case RF_CMD_PUT:
            enctoken(tok, eui64, RF_CMD_PUTACK, seq);
            redis_set(key, val, tok);
        break;
        case RF_CMD_SUB:
            enctoken(tok, eui64, RF_CMD_SUBACK, seq);
            redis_subscribe(key, tok);
        break;
        case RF_CMD_UNSUB:
            enctoken(tok, eui64, RF_CMD_UNSUBACK, seq);
            redis_unsubscribe(key, tok);
        break;
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
            line_init();
	    redis_subscribe("led",NULL);
        break;

        case TCP_EVENT_DISCONNECTED:
            if (connected)
            connected = FALSE;
        break;

        case TCP_EVENT_CANWRITE:
        if (!CMDPKT_ISEMPTY)
        {
            handle_pkt(cmdpkts[cmdpkt_rd].eui64, cmdpkts[cmdpkt_rd].pkt);
            cmdpkt_rd = CMDPKT_NEXT(cmdpkt_rd); // dequeue
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
}

