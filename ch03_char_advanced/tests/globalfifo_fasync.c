#include <stdio.h>
#include <fcntl.h>
#include <signal.h>		/* for signal */
#include <unistd.h>		/* for getpid */
#include <sys/stat.h>

static void signalio_handler(int signum)
{
	printf("receive a signal from globalfifo, signalnum %d\n", signum);
}

int main(void)
{
	int fd, oflags;

	fd = open("/dev/global_mem_0", O_RDWR, S_IRUSR | S_IWUSR);
	
	if (fd != -1) {
		printf("device open success fd:%d\n", fd);
		signal(SIGIO, signalio_handler);
		fcntl(fd, F_SETOWN, getpid());
		oflags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, oflags | FASYNC);
		while (1);
	} else {
		printf("device open failure\n");
	}
	return 0;
}
