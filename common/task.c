#include "common.h"

/*===============================================*/

myTime task_get_time(void)
{
	int32_t now;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	now = ts.tv_sec*1000+ts.tv_nsec/1000000;
	return now;
}

void task_delay(myTime msecs)
{
	struct timeval timeval;
	if(msecs <= 0) return;
	timeval.tv_sec = msecs / 1000;
	timeval.tv_usec = (msecs % 1000) * 1000;
	select(0, NULL, NULL, NULL, &timeval);
}

int myRead_nonblock(int fd, void *p, int n)
{
	int i;
	char *b=(char *)p;
	while(n > 0)
	{
		i = read(fd, b, n);
		if(i > 0) {
			n -= i;
			b += i;
			continue;
		}
		if(i == 0) break;
		if(i < 0) {
			if(errno == EINTR) continue;
			if(errno == EAGAIN || errno == EWOULDBLOCK) break;
			printf("read_nonblock %d error(%d): %s\n", fd, errno, strerror(errno));
			break;
		}
	}
	return (b - (char *)p);
}

int myWrite_nonblock(int fd, void *p, int n)
{
	int i;
	char *b=(char *)p;
	while(n > 0)
	{
		i = write(fd, b, n);
		if(i > 0) {
			n -= i;
			b += i;
			continue;
		}
		if(i == 0) break;
		if (i < 0) {
			if(errno == EINTR) continue;
			if(errno == EAGAIN || errno == EWOULDBLOCK) break;
			printf("write_nonblock %d error(%d): %s\n", fd, errno, strerror(errno));
			break;
		}
	}
	return (b - (char *)p);
}

/*===============================================*/

typedef struct {
	int fd;
	Task_Func callback;
} myFile;

typedef struct {
	myTime period; /*周期*/
	myTime point; /*到期时间点*/
	Task_Func callback;
} myTimer;

#define FILE_NUM_MAX	4
#define TIMER_NUM_MAX	4
static myFile files[FILE_NUM_MAX];
static myTimer timers[TIMER_NUM_MAX];

void task_add_file(int fd, Task_Func callback)
{
	int i, n;

	if((fd == -1)||(callback == NULL)) {
		printf("error: fd=%d, callback=%p\n", fd, callback);
		return;
	}

	for(i=0,n=-1; i<FILE_NUM_MAX; ++i)
	{
		if(files[i].callback == NULL) {
			n = i;
			continue;
		}
		if(files[i].fd == fd) {
			printf("fd %d repeat\n", fd);
			return;
		}
	}
	
	if(n < 0) {
		printf("add file too many\n");
		return;
	}
	files[n].fd = fd;
	files[n].callback = callback;
	return;
}

void task_add_timer(myTime period, Task_Func callback)
{
	int i, n;

	if((period <= 0)||(callback == NULL)) {
		printf("error: period=%d, callback=%p\n", period, callback);
		return;
	}

	for(i=0,n=-1; i<TIMER_NUM_MAX; ++i)
	{
		if(timers[i].callback == NULL) {
			n = i;
			continue;
		}
		if(timers[i].period == period) {
			printf("period %d repeat\n", period);
			return;
		}
	}

	if(n < 0) {
		printf("add timer too many\n");
		return;
	}
	timers[n].period = period;
	timers[n].callback = callback;
	timers[n].point = task_get_time() + timers[n].period;
	return;
}

void task_delete_file(int fd)
{
	int i;
	for(i=0; i<FILE_NUM_MAX; ++i)
	{
		if(files[i].fd == fd) {
			files[i].fd = -1;
			files[i].callback = NULL;
			break;
		}
	}
	return;
}
void task_delete_timer(int period)
{
	int i;
	for(i=0; i<TIMER_NUM_MAX; ++i)
	{
		if(timers[i].period == period) {
			timers[i].period = 0;
			timers[i].callback = NULL;
			break;
		}
	}
	return;
}

static void _check_and_do_task(void)
{
	fd_set rfds;
	int i, e;
	int setsize;
	myTime now, temp, timeout;
	struct timeval to, *pto;

	myTimer *ptimer;
	myFile *pfile;

	/*-----------------------------------------------------------*/
	now = task_get_time();
	timeout = -1;
	for(i=0; i<TIMER_NUM_MAX; ++i)
	{
		ptimer = &(timers[i]);
		if(ptimer->callback == NULL) continue;
		temp = MYTIME_DIFF(ptimer->point, now);
		if(temp <= 0) timeout = 0; /*已经到时间点了*/
		else if((timeout == -1)||(timeout > temp)) timeout = temp;
	}
	if(timeout >= 0) {
		to.tv_sec = timeout/1000;
		to.tv_usec = (timeout%1000)*1000;
		pto = &to;
	} else {
		pto = NULL; /*永远等待*/
	}
	/*----------------------------------------------------------*/
	FD_ZERO(&rfds);
	setsize = 0;
	for(i=0; i<FILE_NUM_MAX; ++i)
	{
		pfile = &(files[i]);
		if(pfile->callback == NULL) continue;
		FD_SET(pfile->fd, &rfds);
		if(setsize < pfile->fd)
			setsize = pfile->fd;
	}

	errno = 0;
	e = select(setsize+1, &rfds, NULL, NULL, pto);
	if((e<0)&&(errno != EINTR)&&(errno != EAGAIN)&&(errno != EWOULDBLOCK))
	{
		printf("select error(%d): %s \n", errno, strerror(errno));
		task_delay(1000);
		return;
	}
	/*-----------------------------------------------------------*/
	if(e > 0)
	{
		for(i=0; i<FILE_NUM_MAX; ++i)
		{
			pfile = &(files[i]);
			if(pfile->callback == NULL) continue;
			if(FD_ISSET(pfile->fd, &rfds))
				pfile->callback(pfile->fd);
		}
	}

	now = task_get_time();
	for(i=0; i<TIMER_NUM_MAX; ++i)
	{
		ptimer = &(timers[i]);
		if(ptimer->callback == NULL) continue;
		temp = MYTIME_DIFF(ptimer->point, now);
		if(temp <= 0) { /*已经到时间点了*/
			ptimer->point = now + ptimer->period;
			ptimer->callback(ptimer->period);
		}
	}
	return;
}

void task_loop(void)
{
	while(1) {
		_check_and_do_task();
	}
	return;
}
