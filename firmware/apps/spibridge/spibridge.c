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
#include "net.h"
#include "pkt.h"

#include "itoa.h"

#define DEBUG 1

#define dma_arm_delay()  \
       	__asm \
	nop \
	nop \
	nop \
	nop \
	nop \
	nop \
	nop \
	nop \
	nop \
	__endasm

static __xdata uint8_t local_seq;

#ifdef CRYPTO_ENABLED
static __xdata uint8_t encpkt[128];
#endif

static __xdata uint8_t rambuf[128];

static __xdata struct cc_dma_channel dma_config[5];

struct cc_dma_channel
{
	uint8_t src_high;
	uint8_t src_low;
	uint8_t dst_high;
	uint8_t dst_low;
	uint8_t len_high;
	uint8_t len_low;
	uint8_t cfg0;
	uint8_t cfg1;
};



#define DMA_CFG0_TRIGGER_NONE      0
#define DMA_CFG0_TRIGGER_PREV      1
#define DMA_CFG0_TRIGGER_T1_CH0    2
#define DMA_CFG0_TRIGGER_T1_CH1    3
#define DMA_CFG0_TRIGGER_T1_CH2    4
#define DMA_CFG0_TRIGGER_T2_OVFL   6
#define DMA_CFG0_TRIGGER_T3_CH0    7
#define DMA_CFG0_TRIGGER_T3_CH1    8
#define DMA_CFG0_TRIGGER_T4_CH0    9
#define DMA_CFG0_TRIGGER_T4_CH1    10
#define DMA_CFG0_TRIGGER_IOC_0     12
#define DMA_CFG0_TRIGGER_IOC_1     13
#define DMA_CFG0_TRIGGER_URX0      14
#define DMA_CFG0_TRIGGER_UTX0      15
#define DMA_CFG0_TRIGGER_URX1      16
#define DMA_CFG0_TRIGGER_UTX1      17
#define DMA_CFG0_TRIGGER_FLASH     18
#define DMA_CFG0_TRIGGER_RADIO     19
#define DMA_CFG0_TRIGGER_ADC_CHALL 20
#define DMA_CFG0_TRIGGER_ADC_CH0   21
#define DMA_CFG0_TRIGGER_ADC_CH1   22
#define DMA_CFG0_TRIGGER_ADC_CH2   23
#define DMA_CFG0_TRIGGER_ADC_CH3   24
#define DMA_CFG0_TRIGGER_ADC_CH4   25
#define DMA_CFG0_TRIGGER_ADC_CH5   26
#define DMA_CFG0_TRIGGER_ADC_CH6   27
#define DMA_CFG0_TRIGGER_ADC_CH7   28
#define DMA_CFG0_TRIGGER_I2SRX     27
#define DMA_CFG0_TRIGGER_I2STX     28
#define DMA_CFG0_TRIGGER_ENC_DW    29
#define DMA_CFG0_TRIGGER_ENC_UP    30

#define DMA_CFG1_SRCINC_0      (0 << 6)
#define DMA_CFG1_SRCINC_1      (1 << 6)
#define DMA_CFG1_DESTINC_0     (0 << 4)
#define DMA_CFG1_DESTINC_1     (1 << 4)
#define DMA_CFG1_PRIORITY_HIGH     (2 << 0)
#define DMAARM_DMAARM0         (1 << 0)
#define DMAARM_DMAARM1         (1 << 1)
#define DMAARM_DMAARM2         (1 << 2)
#define DMAARM_DMAARM3         (1 << 3)
#define DMAARM_DMAARM4         (1 << 4)

#define DMA_LEN_HIGH_VLEN_MASK     (7 << 5)
#define DMA_LEN_HIGH_VLEN_LEN      (0 << 5)
#define DMA_LEN_HIGH_VLEN_PLUS_1   (1 << 5)
#define DMA_LEN_HIGH_VLEN      (2 << 5)
#define DMA_LEN_HIGH_VLEN_PLUS_2   (3 << 5)
#define DMA_LEN_HIGH_VLEN_PLUS_3   (4 << 5)
#define DMA_LEN_HIGH_MASK      (0x1f)

#define DMA_CFG0_WORDSIZE_8        (0 << 7)
#define DMA_CFG0_WORDSIZE_16       (1 << 7)
#define DMA_CFG0_TMODE_MASK        (3 << 5)
#define DMA_CFG0_TMODE_SINGLE      (0 << 5)
#define DMA_CFG0_TMODE_BLOCK       (1 << 5)
#define DMA_CFG0_TMODE_REPEATED_SINGLE (2 << 5)
#define DMA_CFG0_TMODE_REPEATED_BLOCK  (3 << 5)

#define U1DBUF_ADDR 0xDFF9

void app_init(void)
{
	local_seq = 0;
	radio_rx();

	// UART1 spi alt2 used in slave mode.
	P1SEL |= (BIT4 | BIT5 | BIT6 | BIT7 );
	P1INP |= (BIT4 | BIT5 | BIT6 );
	U1CSR = U1CSR_RE | U1CSR_SLAVE;
	U1GCR = U1GCR_ORDER; // Select MSB witch is default in wiringPi lib
	PERCFG |= PERCFG_U1CFG;

	// Setup DMA descriptor for incoming SPI data
	dma_config[0].src_high  = (((uint16_t)(__xdata uint16_t *)U1DBUF_ADDR) >> 8) & 0x00FF;
	dma_config[0].src_low   = ((uint16_t)(__xdata uint16_t *)U1DBUF_ADDR) & 0x00FF;
	dma_config[0].dst_high  = (((uint16_t)(__xdata uint16_t *)rambuf) >> 8) & 0x00FF;
	dma_config[0].dst_low   = ((uint16_t)(__xdata uint16_t *)rambuf) & 0x00FF;
	dma_config[0].len_high  = DMA_LEN_HIGH_VLEN_PLUS_1;
	//dma_config[0].len_high |= (0 >> 8) & DMA_LEN_HIGH_MASK;
	dma_config[0].len_low   = (128) & 0x00FF;
	dma_config[0].cfg0 = DMA_CFG0_WORDSIZE_8 | DMA_CFG0_TMODE_SINGLE | DMA_CFG0_TRIGGER_URX1;
	dma_config[0].cfg1 = DMA_CFG1_SRCINC_0 | DMA_CFG1_DESTINC_1 | DMA_CFG1_PRIORITY_HIGH;

	// Setup DMA descriptor for outgoing SPI data
	dma_config[1].src_high  = (((uint16_t)(__xdata uint16_t *)rambuf) >> 8) & 0x00FF;
	dma_config[1].src_low   = ((uint16_t)(__xdata uint16_t *)rambuf) & 0x00FF;
	dma_config[1].dst_high  = (((uint16_t)(__xdata uint16_t *)U1DBUF_ADDR) >> 8) & 0x00FF;
	dma_config[1].dst_low   = ((uint16_t)(__xdata uint16_t *)U1DBUF_ADDR) & 0x00FF;
	dma_config[1].len_high  = DMA_LEN_HIGH_VLEN_PLUS_3;
	//dma_config[0].len_high |= (0 >> 8) & DMA_LEN_HIGH_MASK;
	dma_config[1].len_low   = (128) & 0x00FF;
	dma_config[1].cfg0 = DMA_CFG0_WORDSIZE_8 | DMA_CFG0_TMODE_SINGLE | DMA_CFG0_TRIGGER_UTX1;
	dma_config[1].cfg1 = DMA_CFG1_SRCINC_1 | DMA_CFG1_DESTINC_0 | DMA_CFG1_PRIORITY_HIGH;

	// Point DMA controller at our DMA descriptor
	DMA0CFGH = ((uint16_t)&dma_config[0] >> 8) & 0x00FF;
	DMA0CFGL = (uint16_t)&dma_config[0] & 0x00FF;
	DMA1CFGH = ((uint16_t)&dma_config[1] >> 8) & 0x00FF;
	DMA1CFGL = (uint16_t)&dma_config[1] & 0x00FF;

	DMAARM |= DMAARM_DMAARM0;
	//DMAREQ = BIT1;
	//IEN1 |= DMAIE 

	// Set port 2_1 to output for interupt to master
	P2DIR |= (BIT1 | BIT2);
	P2=0;

}

void dma_isr(void) {
}


void radio_received(__xdata uint8_t *pkt) {
#if DEBUG
	uint8_t i;
	cons_puts("RX: ");
	for (i=0;i<pkt[0]+1;i++)
		cons_puthex8(pkt[i]);
	cons_puthex8(RSSI);
	cons_puthex8(LQI&0x7F);
	cons_puts("\r\n");
#endif
	pkt[pkt[0]+1]=RSSI;
	pkt[pkt[0]+2]=LQI&0x7F;

	dma_config[1].src_high  = (((uint16_t)(__xdata uint16_t *)pkt) >> 8) & 0x00FF;
	dma_config[1].src_low   = ((uint16_t)(__xdata uint16_t *)pkt) & 0x00FF;
	DMAARM = DMAARM_DMAARM0 | DMAARM_ABORT; // abort reciving DMA
	DMAARM = DMAARM_DMAARM1;
	dma_arm_delay();
	DMAREQ = BIT1;
	P2_1=1;
	P2_1=0;
}

void radio_idle_cb(void) {
	radio_rx();
}

void app_tick(void) {
	uint8_t ch;
	uint8_t i;

	if (cons_getch(&ch)) {
		switch (ch) {
			case 'd':
				cons_puts("RBUF: ");
				for (i=0;i<rambuf[0]+1;i++)
					cons_puthex8(rambuf[i]);
				cons_puts("\r\n");
				//DMAARM |= DMAARM_DMAARM0;
				//DMAREQ = BIT1;
				break;
			case '1':
				cons_puts("DMA_CFG: ");
				cons_puthex8(dma_config[1].src_high);
				cons_puthex8(dma_config[1].src_low);
				cons_puthex8(dma_config[1].dst_high);
				cons_puthex8(dma_config[1].dst_low);
				cons_puthex8(dma_config[1].len_high);
				cons_puthex8(dma_config[1].len_low);
				cons_puthex8(dma_config[1].cfg0);
				cons_puthex8(dma_config[1].cfg1);
				cons_puts("\r\n");
				break;

			case '2':
				P2_1=0;
				break;

			case '3':
				P2_1=1;
				break;

			case '4':
				P2_2=0;
				break;

			case '5':
				P2_2=1;
				break;

		}
	}
	if (DMAIRQ & DMAIRQ_DMAIF0) { // we have recived packet from SPI
		DMAIRQ &= ~DMAIRQ_DMAIF0;
#if DEBUG
		cons_puts("DMA_IRQ channel 0\r\n");
#endif
		switch(rambuf[1]) {
			case 1:
				radio_tx((__xdata uint8_t *)rambuf+2);
				break;
			case 2:
#if CRYPTO_ENABLED
				PKTHDR(encpkt)->length = rambuf[2]+1;
				PKTHDR(encpkt)->dst_id = rambuf[3];
				PKTHDR(encpkt)->src_id = rambuf[4];
				memcpy(PKTPAYLOAD(encpkt), rambuf+2, rambuf[2]+1);
				pkt_enc(encpkt, config_getKeyEnc(), config_getKeyMac());
				memcpy(rambuf+2, encpkt, encpkt[0]+1);
#endif
				radio_tx((__xdata uint8_t *)rambuf+2);
				break;
		}
		DMAARM = DMAARM_DMAARM0; // arm to accept packet from SPI. 
	}
	if (DMAIRQ & DMAIRQ_DMAIF1) { // Packet sending to SPI done arm recive dma
		DMAIRQ &= ~DMAIRQ_DMAIF1;
#if DEBUG
		cons_puts("DMA_IRQ channel 1\r\n");
#endif
		DMAARM = DMAARM_DMAARM0; // arm to accept packet from SPI. 
	}
}

void app_10hz(void)
{
}

void app_100hz(void)
{
}

void app_1hz(void)
{
}
