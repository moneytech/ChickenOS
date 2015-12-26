/* ChickenOS PIT and RTC routines
 *
 * TODO: Move much of this to arch/i386/pit.c and device/time.c
 *
 */
#include <kernel/timer.h>
#include <kernel/interrupt.h>
#include <kernel/common.h>
#include <kernel/thread.h>
#include <kernel/hw.h>
#include <sys/time.h>
#include <stdio.h>

#define PIT0_DATA 0x40
#define PIT1_DATA 0x41
#define PIT2_DATA 0x42
#define PIT_CMD   0x43

#define RTC_REG    0x70
#define RTC_DATA   0x71
#define RTC_UPDATE 0x80

#define BCD_TO_DEC(x) (((x / 16) * 10) + (x & 0xf))

struct c_os_time system_datetime;
int rtc_format = 0;
uint32_t ticks = 0;
uint32_t unix_time;

char * days[7] = {
	"Sunday", "Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

uint8_t read_rtc_reg(uint8_t reg)
{
/*
	int chk = 0;
start:
	outb(RTC_REG, 0xA);
	chk = inb(RTC_REG);
	if(!(chk&RTC_UPDATE))
		goto start;
*/
	outb(RTC_REG, reg);

	uint8_t ret = inb(RTC_DATA);
	if (rtc_format == 2)
		return BCD_TO_DEC(ret);
	return ret;
}

// Gregorian date to Julian day function
// taken from:http://www.hermetic.ch/cal_stud/jdn.htm#comp
int32_t jdt(int8_t m, int8_t d, int16_t y)
{
	int32_t jd = ( 1461 * ( y + 4800 + ( m - 14 ) / 12 ) ) / 4 +
		( 367 * ( m - 2 - 12 * ( ( m - 14 ) / 12 ) ) ) / 12 -
		( 3 * ( ( y + 4900 + ( m - 14 ) / 12 ) / 100 ) ) / 4 +
		d - 32075;

	return jd;
}

void time_print(struct c_os_time *time)
{
	printf("%s ",days[time->weekday]);
	printf("%i/%i/%i ", time->month, time->day, time->year+2000);
	printf("%i:%i:%i\n", time->hour, time->minute, time->second);
}

void time_set_from_rtc(struct c_os_time *time)
{
	uint32_t julian_day = jdt(time->month, time->day,time->year+2000);
	uint32_t days_since_epoch = (julian_day - jdt(1,1,1970));
	uint32_t seconds_since_midnight = time->hour*60*60 + time->minute*60 + time->second;
	unix_time = (days_since_epoch * 24 * 60 * 60) + seconds_since_midnight;
	time->weekday = (julian_day + 1) % 7;
	time_print(time);
}

void rtc_init()
{
	rtc_format = read_rtc_reg(0xB);

	system_datetime.second = read_rtc_reg(0);
	system_datetime.minute = read_rtc_reg(2);
	system_datetime.hour = read_rtc_reg(4);
	//Should be Weekday, doesn't work correctly
	system_datetime.weekday = read_rtc_reg(6);

	system_datetime.day = read_rtc_reg(7);
	system_datetime.month = read_rtc_reg(8);
	system_datetime.year = read_rtc_reg(9);
	system_datetime.century = read_rtc_reg(0x32);
	//XXX: Is this right?
	if(system_datetime.hour == 0) system_datetime.hour = 12;
	if(system_datetime.hour > 12) system_datetime.hour -= 12;

	time_set_from_rtc(&system_datetime);
}

void timer_intr(struct registers * regs)
{
	(void)regs;
	ticks++;
	if(ticks % 100 == 0)
	{
		unix_time++;
	}
	thread_scheduler(regs);
}

void timer_init(uint32_t frequency)
{
	int div = 1193180 / frequency;

	outb(PIT_CMD, 0x36);
	outb(PIT0_DATA, div & 0xFF);
	outb(PIT0_DATA, div >> 8);

	interrupt_register(IRQ0, &timer_intr);
}

void time_sleep(int seconds)
{
	uint32_t wait = seconds + unix_time;
	while(unix_time <  wait);
	return;
}

void time_msleep(int mseconds)
{
	uint32_t wait = mseconds + ticks;
	while(ticks <  wait);
	return;
}

void time_usleep(int useconds)
{
	uint32_t wait = useconds/10 + ticks;
	while(ticks <  wait);
	return;
}

void time_init()
{
	rtc_init();
	timer_init(100);
}

int sys_gettimeofday(struct timeval *tp, void *tzp UNUSED)
{
	if(tp != NULL)
	{
		tp->tv_sec = unix_time;
		tp->tv_usec = (ticks % 100) * 100;
	}

	return 0;
}

int sys_clock_gettime(int type, struct timespec *tp)
{
	(void)type;
	static int s = 0;
	tp->tv_sec = unix_time;
	tp->tv_nsec = s;
	s += 100;
	unix_time++;
	return 0;
}
