#include <stdio.h>
#include "../common/common.h"

#define RED	FB_COLOR(255,0,0)
#define ORANGE	FB_COLOR(255,165,0)
#define YELLOW	FB_COLOR(255,255,0)
#define GREEN	FB_COLOR(0,255,0)
#define CYAN	FB_COLOR(0,127,255)
#define BLUE	FB_COLOR(0,0,255)
#define PURPLE	FB_COLOR(139,0,255)
#define BLACK   FB_COLOR(0,0,0)

#define COLOR_BACKGROUND	FB_COLOR(0xff,0xff,0xff)  //white

static int touch_fd;
static int radius[5] = {45,47,49,51,53};
static struct old_pos{
	int x;
	int y;
} old[5];

static void touch_event_cb(int fd)
{
	int type,x,y,finger, color;
	type = touch_read(fd, &x,&y,&finger);
	switch(type){
	case TOUCH_PRESS:
		printf("TOUCH_PRESS:x=%d,y=%d,finger=%d\n",x,y,finger);
		switch(finger){
			case 0: color = ORANGE;break;
			case 1: color = BLUE;break;
			case 2: color = RED;break;
			case 3: color = GREEN;break;
			case 4: color = BLACK;break;
			default: break;
		}
		fb_draw_circle(x, y, radius[finger], color);
		old[finger].x = x;
		old[finger].y = y;
		break;
	case TOUCH_MOVE:
		switch(finger){
			case 0: color = ORANGE;break;
			case 1: color = BLUE;break;
			case 2: color = RED;break;
			case 3: color = GREEN;break;
			case 4: color = BLACK;break;
			default: break;
		}
		fb_draw_circle(old[finger].x, old[finger].y, radius[finger]+2, COLOR_BACKGROUND);
		fb_draw_circle(x, y, radius[finger], color);
		old[finger].x = x;
		old[finger].y = y;
		break;
	case TOUCH_RELEASE:
		printf("TOUCH_RELEASE:x=%d,y=%d,finger=%d\n",x,y,finger);
		fb_draw_circle(x, y, radius[finger]+2, COLOR_BACKGROUND);
		break;
	case TOUCH_ERROR:
		printf("close touch fd\n");
		close(fd);
		task_delete_file(fd);
		break;
	default:
		return;
	}
	fb_update();
	return;
}

int main(int argc, char *argv[])
{
	fb_init("/dev/fb0");
	fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,COLOR_BACKGROUND);
	fb_update();

	//打开多点触摸设备文件, 返回文件fd
	touch_fd = touch_init("/dev/input/event2");
	//添加任务, 当touch_fd文件可读时, 会自动调用touch_event_cb函数
	task_add_file(touch_fd, touch_event_cb);
	
	task_loop(); //进入任务循环
	return 0;
}
