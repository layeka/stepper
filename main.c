#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"
#include "timer.h"

#define NM 4
#define NS 8

volatile uint8_t VERSION = 1;

volatile uint8_t *mport[NM];
volatile uint8_t *mddr[NM];
volatile uint8_t mbits[NM];

volatile uint8_t *spin[NS] = 
	{&PINC,&PINC,&PINC,&PINC, &PINC,&PINC,&PINC,&PINC};

volatile uint8_t *sport[NS] =
	{&PORTC,&PORTC,&PORTC,&PORTC, &PORTC,&PORTC,&PORTC,&PORTC};

volatile uint8_t sbit[NS] = {0,1,2,3,4,5,6,7};
volatile uint8_t sok = 0xff;

volatile uint8_t mot2sens[NM] = {0,0,0,0};

uint8_t sns;

/* 256 - 12e3/64 */
#define TIM_START_VAL 69

volatile uint8_t del;
volatile int16_t nsteps[NM];
volatile uint16_t nsteps_completed[NM];
volatile uint8_t pout[NM];
volatile uint8_t t[NM]; /* timers */

volatile uint32_t timval, timval_copy;


ISR(TIMER0_OVF_vect)
{
	unsigned int i;
	TCNT0 = TIM_START_VAL;
	for (i = 0; i < NM; i++)
		if (t[i])
			t[i]--;
}

void set_mport(uint8_t i, uint8_t val)
{
	uint8_t p;
	p = *mport[i];
	p &= ~(0xf << mbits[i]);
	p |= (val << mbits[i]);  
	*mport[i] = p;
}

/* handle usb requests */
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *)data;
	unsigned int i;

	i = rq->wIndex.word & 0xf;
	if ((rq->bRequest < 0x80 && i >= NM) ||
		(rq->bRequest > 0x7f && i>=NS))
		return 0;

	switch (rq->bRequest) {
	case 0: /* go! */
		if (!nsteps[i]){
			nsteps[i] = rq->wValue.word;
			nsteps_completed[i] = 0;
		}
		break;

	case 1: /* report number of steps left to step */
		usbMsgPtr = (uchar *)&nsteps[i];
		return sizeof(*nsteps);

	case 2: /* set delay */
		del = rq->wValue.bytes[0];
		break;

	case 3: /* stop */
		nsteps[i] = 0;
		break;

	case 4: /* power down */
		nsteps[i]=0;
		set_mport(i, 0);
		break;

	case 5: /* power up */
		set_mport(i, pout[i]);
		break;

	case 6: /* report number of completed steps */
		usbMsgPtr = (uchar *)&nsteps_completed[i];
		return sizeof(*nsteps_completed);

	case 0x80: /* report i-th sensor value */
		if ((*spin[i]) & (1 << sbit[i]))
			sns=1;
		else
			sns=0;
		usbMsgPtr = &sns;
		return sizeof(sns);

	case 0x81: /* rebind sensor to mot */
		mot2sens[i] = rq->wValue.bytes[0];

	case 0x82: /* reset sensor ok-value */
		if (*spin[mot2sens[i]] & (1 << sbit[mot2sens[i]]))
			sok |= 1 << mot2sens[i];
		else
			sok &= ~(1 << mot2sens[i]);
		break;

	case 0xc0: /* report timer value */
		timval_copy = timval;
		usbMsgPtr = (uchar *)&timval_copy;
		return sizeof(timval_copy);

	case 0xff: /* return version */
		if (VERSION == rq->wValue.bytes[0])
			VERSION = 0;
		usbMsgPtr = (uchar *)&VERSION;
		return sizeof(VERSION);
	}
	return 0;
}

int main(void)
{
	unsigned int i, is;
	TIMER_TYPE tmr = -1;

	del = 5; /* ms */

	/* setup motor ports and sensors */
	mport[0] = &PORTA;
	mddr[0] = &DDRA;
	mbits[0] = 0;

	mport[1] = &PORTA;
	mddr[1] = &DDRA;
	mbits[1] = 4;

	mport[2] = &PORTB;
	mddr[2] = &DDRB;
	mbits[2] = 0;

	mport[3] = &PORTB;
	mddr[3] = &DDRB;
	mbits[3] = 4;

	timer_init();
	sei();
	tmr = timer_start();
	_delay_ms(10);
	timval = timer_stop(tmr);
	cli();

	/* setup usb */
	usbInit();
	/* enforce re-enumeration, do this while interrupts are disabled! */
	usbDeviceDisconnect();
	_delay_ms(250);
	usbDeviceConnect();

	/* timer setup */
	TIMSK |= (1 << TOIE0); /* enable int on timer overflow */
	TCNT0 = TIM_START_VAL;
	TCCR0 |= (1 << CS01) | (1 << CS00); /* prescaler 1/64 */


	/* Motor control port setup */
	for (i = 0; i < NM; i++) {
		*mddr[i] |= (0xf << mbits[i]);
		pout[i] = 1;
		set_mport(i, pout[i]);
		t[i] = 0;
	}
	for (i = 0; i < NS; i++) 
		*sport[i] |= 1 << sbit[i];

	sei(); /* enable interrupts */

	while(1) { /* main loop */

		usbPoll(); /* handle usb events */

		/* prevent usage by incompatible host software */
		if (VERSION)
			continue;

		for (i = 0; i < NM; i++) {
			/* check terminal sensors */
			is = mot2sens[i];
			if ((((sok & (1 << is)) >> is) << sbit[is]) ^
				(*spin[is] & (1 << sbit[is])))
				nsteps[i]=0;

			if (nsteps[i] && !t[i]) {
				if (nsteps[i]>0)
					pout[i] = pout[i] << 1;
				else 
					pout[i] = pout[i] >> 1;

				if (pout[i] == 0)
					pout[i] = 8;

				if (pout[i] > 8)
					pout[i] = 1;

				nsteps[i] > 0 ? nsteps[i]-- : nsteps[i]++;

				nsteps_completed[i]++;

				set_mport(i, pout[i]);

				t[i] = del;
			}
		}
	}
}

