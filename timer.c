#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "timer.h"

struct timer {
	uint8_t active;
	uint16_t start;
	uint32_t count;
};

static volatile struct timer timers[TIMER_NUM];

void timer_init()
{
	TIMER_TYPE t;

	for (t = 0; t < TIMER_NUM; t++)
		timers[t].active = 0;

	TIMSK |= 1 << TOIE1;
	TCCR1B |= 1 << CS10;
}

TIMER_TYPE timer_start()
{
	TIMER_TYPE t;

	for (t = 0; t < TIMER_NUM; t++) {
		if (!timers[t].active)
			break;
	}
	if (t == TIMER_NUM)
		return -1;
	timers[t].count = 0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		timers[t].start = TCNT1;
		timers[t].active = 1;
	}
	return t;
}

uint32_t timer_stop(TIMER_TYPE t)
{
	uint32_t ret = 0;
	uint16_t tcnt;

	if (t >= 0 && t < TIMER_NUM) {
		timers[t].active = 0;
		tcnt = TCNT1;
		timers[t].count += tcnt > timers[t].start ?
				   tcnt - timers[t].start :
				   tcnt + 0x10000ull - timers[t].start;
		ret = timers[t].count;
	}
	return ret;
}

ISR(TIMER1_OVF_vect)
{
	TIMER_TYPE t;

	for (t = 0; t < TIMER_NUM; t++) {
		if (timers[t].active) {
			timers[t].count += 0x10000ull - timers[t].start;
			timers[t].start = 0;
		}
	}
}
