#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd;
	int counter = 0;
	int old_counter = 0;
	ssize_t n;

	/* 打开 /dev/second 设备文件 */
	fd = open("/dev/second", O_RDONLY);
	if (fd != - 1) {
		while (1) {
			n = read(fd, &counter, sizeof(counter));
			if (n != (ssize_t)sizeof(counter))
				break;
			if(counter!=old_counter) {
				printf("seconds after open /dev/second :%d\n",counter);
				old_counter = counter;
			}
		}
	} else {
		printf("Device open failure\n");
	}
	return 0;
}
