#define TIMER_NUM	1
#define TIMER_TYPE	int

void timer_init();
TIMER_TYPE timer_start();
uint32_t timer_stop(TIMER_TYPE);
