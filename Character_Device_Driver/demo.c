// demo.c - print keystrokes decoded by the ps2kbd driver. Ctrl-C to quit.
//   make demo && sudo ./demo

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define DEVICE "/dev/ps2kbd"

int main(void)
{
	char buf[64];
	int fd = open(DEVICE, O_RDONLY);

	if (fd < 0) {
		perror("open " DEVICE);
		return 1;
	}
	fputs("reading keystrokes (Ctrl-C to quit)...\n", stderr);

	for (;;) {
		ssize_t n = read(fd, buf, sizeof buf);

		if (n == 0)		// driver stopped feeding us
			break;
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("read");
			close(fd);
			return 1;
		}
		fwrite(buf, 1, n, stdout);
		fflush(stdout);
	}

	close(fd);
	return 0;
}
