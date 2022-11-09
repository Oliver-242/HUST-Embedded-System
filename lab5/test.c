#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void bluetooth_tty_read(int fd)
{
	char buf[128];
	int n;

	n = read(fd, buf, sizeof(buf)-1);
	if(n <= 0) {
		printf("read error\n");
		exit(0);
		return;
	}

	buf[n] = '\0';
	printf("bluetooth tty read \"%s\"\n", buf);
	return;
}

static void bluetooth_tty_write(int fd)
{
        int n;
        n = write(fd, "ok\n", 3);
        if(n != 3) {
                printf("write error\n");
                exit(0);
                return;
        }
        return;
}

static int bluetooth_tty_init(const char *dev)
{
	int fd = open(dev, O_RDWR|O_NOCTTY);
	if(fd < 0){
		printf("bluetooth_tty_init open %s error(%d): %s\n", dev, errno, strerror(errno));
		return -1;
	}
	return fd;
}


int main(int argc, char *argv[])
{
	int bluetooth_fd;

	bluetooth_fd = bluetooth_tty_init("/dev/rfcomm0");
	if(bluetooth_fd == -1) return 0;

	bluetooth_tty_write(bluetooth_fd);

	while(1) bluetooth_tty_read(bluetooth_fd);
	return 0;
}

