#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"

#define NM 2
#define NS 4

volatile uint8_t * mport[NM];
volatile uint8_t * mddr[NM];
volatile uint8_t * spin[NS];
volatile uint8_t * sport[NS];
volatile uint8_t sbit[NS];
volatile uint8_t mbits[NM]; // = {0, 4};

// 256 - 12e3/64
#define TIM_START_VAL 69

volatile uint8_t del;
volatile uint8_t dir;
volatile uint8_t nsteps[NM];
volatile uint8_t nsteps_completed[NM];
volatile uint8_t pout[NM];
volatile uint8_t t[NM]; // timers


ISR(TIMER0_OVF_vect){
  unsigned int i;
  TCNT0 = TIM_START_VAL;
  for (i=0; i<NM; i++){
    if (t[i]) t[i]--;
  }
}

void set_mport(uint8_t i, uint8_t val){
  uint8_t p;
  p = *mport[i];
  p &= ~(0xf << mbits[i]);
  p |= (val << mbits[i]);  
  *mport[i] = p;
}

usbMsgLen_t usbFunctionSetup(uchar data[8]) { // handle usb requests
  usbRequest_t *rq = (void *)data;
  unsigned int i;
  uint8_t sns=0;

  i = (rq->wValue.bytes[1] & 0xe)>>1;
  if ((rq->bRequest < 0x80 && i >= NM) || (rq->bRequest > 0x7f && i>=NS) ) return 0;
  switch (rq->bRequest){
    case 0: // step  (uint16_t) Value = empty[4] i_motor[3] dir[1]  nsteps[8]
      if (!nsteps[i]){
        if (rq->wValue.bytes[1] & 0x1)
          dir |= 1<<i;
        else
          dir &= ~(1<<i);
        nsteps[i] = rq->wValue.bytes[0];
        nsteps_completed[i] = 0;
      }
      break;

    case 1: // report number of steps left to step
      usbMsgPtr = &nsteps[i];
      return sizeof(*nsteps);

    case 2: // set delay
      del = rq->wValue.bytes[0];
      break;

    case 3: // stop
      nsteps[i] = 0;
      break;

    case 4: // power down
      set_mport(i, 0);
      break;

    case 5: // power up
      set_mport(i, pout[i]);
      break;

    case 6: // report number of completed steps
      usbMsgPtr = &nsteps_completed[i];
      return sizeof(*nsteps_completed);

    case 0x80: // report i-th sensor value
      if ( *spin[i] & (1 << sbit[i])) sns=1;
      usbMsgPtr = &sns;
      return sizeof(sns);
      break;
  }
  return 0;
}

void main(void){
  unsigned int i;
  del = 5; // ms

// setup motor ports and sensors
  mport[0] = &PORTC;
  mddr[0] = &DDRC;
  mbits[0] = 0;
  spin[0] = &PINC;
  sport[0] = &PORTC;
  sbit[0] = 4;
  spin[1] = &PINC;
  sport[1] = &PORTC;
  sbit[1] = 5;

  mport[1] = &PORTB;
  mddr[1] = &DDRB;
  mbits[1] = 0;
  spin[2] = &PINB;
  sport[2] = &PORTB;
  sbit[2] = 4;
  spin[3] = &PINB;
  sport[3] = &PORTB;
  sbit[3] = 5;


// setup usb
  usbInit();
  usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
  _delay_ms(250);
  usbDeviceConnect();

// timer setup
  TIMSK |= (1 << TOIE0); // enable int on timer overflow
  TCNT0 = TIM_START_VAL;
  TCCR0 |= (1<<CS01)|(1<<CS00); // prescaler 1/64

// Motor control port setup
  for (i=0; i<NM; i++) {
    *mddr[i] |= (0xf<<mbits[i]);
    pout[i] = 1;
    set_mport(i, pout[i]);
    t[i] = 0;
    *sport[2*i] |= 1<<sbit[2*i];
    *sport[2*i+1] |= 1<<sbit[2*i+1];
  }

  sei(); // enable interrupts

  while(1){ // main loop
    for (i=0; i<NM; i++){

//      check terminal sensors here

      if ( nsteps[i] && !t[i] ){
        if (dir & (1 << i)){
          if (*spin[2*i] & (1<<sbit[2*i]))
            pout[i] = pout[i] << 1;
          else
            nsteps[i] = 1;
        }
        else {
          if (*spin[2*i+1] & (1<<sbit[2*i+1]))
            pout[i] = pout[i] >> 1;
          else
            nsteps[i] = 1;
        }
        if (pout[i] == 0) pout[i] = 8;
        if (pout[i] > 8 ) pout[i] = 1;

        nsteps[i]--;
        nsteps_completed[i]++;
        set_mport(i, pout[i]);
        t[i] = del;
      }
    }

    usbPoll(); // handle usb events

  }
}

