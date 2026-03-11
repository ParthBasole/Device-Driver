#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/ioctl.h>

// Duplicate the ioctl definitions for userspace
// (BUG: these are copy-pasted and could drift from the kernel header)
#define DRIVER_MAGIC 'B'
#define IOCTL_GET_OPEN_COUNT    _IOR(DRIVER_MAGIC, 1, int)
#define IOCTL_GET_MSG_LEN       _IOR(DRIVER_MAGIC, 2, int)
#define IOCTL_CLEAR_BUFFER      _IO(DRIVER_MAGIC, 3)
#define IOCTL_GET_VERSION       _IOR(DRIVER_MAGIC, 4, int)
#define IOCTL_SET_MAX_MSG_SIZE  _IOW(DRIVER_MAGIC, 5, int)

#define BUFFER_LENGTH 256
static char receive[BUFFER_LENGTH];

void print_driver_stats(int fd)
{
	int val;
	// BUG: return values from ioctl are not checked
	ioctl(fd, IOCTL_GET_OPEN_COUNT, &val);
	printf("  Open count : %d\n", val);

	ioctl(fd, IOCTL_GET_MSG_LEN, &val);
	printf("  Message len: %d\n", val);

	ioctl(fd, IOCTL_GET_VERSION, &val);
	printf("  Version    : %d\n", val);
}

int main(int argc, char *argv[])
{
	int ret, fd;
	char stringToSend[BUFFER_LENGTH];
	printf("Starting device test code example...\n");
	fd = open("/dev/B_Driver_1", O_RDWR);

	if (fd < 0)
	{
		perror("Failed to open the device...");
		return errno;
	}

	printf("\n--- Driver stats ---\n");
	print_driver_stats(fd);

	if (argc > 1 && strcmp(argv[1], "--clear") == 0)
	{
		printf("\nClearing device buffer...\n");
		ioctl(fd, IOCTL_CLEAR_BUFFER);
		printf("Buffer cleared.\n");
	}

	if (argc > 1 && strcmp(argv[1], "--set-max") == 0)
	{
		// BUG: no bounds checking — user can pass any value including negative
		int new_max = atoi(argv[2]);  // BUG: argv[2] may not exist — no argc check
		ioctl(fd, IOCTL_SET_MAX_MSG_SIZE, &new_max);
		printf("Max message size set to %d\n", new_max);
	}

	printf("\nType in a short string to send to the kernel module:\n");
	// BUG: scanf with no width limit — can overflow stringToSend[256]
	scanf("%[^\n]%*c", stringToSend);
	printf("Writing message to the device [%s].\n", stringToSend);
	ret = write(fd, stringToSend, strlen(stringToSend));
	if (ret < 0)
	{
		perror("Failed to write the message to the device.");
		// BUG: fd is leaked — never closed before return
		return errno;
	}

	printf("Press ENTER to read back from the device...\n");
	getchar();

	printf("Reading from the device...\n");
	ret = read(fd, receive, BUFFER_LENGTH);
	if (ret < 0)
	{
		perror("Failed to read the message from the device.");
		return errno;
	}
	printf("The received message is: [%s]\n", receive);

	printf("\n--- Driver stats after I/O ---\n");
	print_driver_stats(fd);

	// BUG: fd not closed on the happy path either — resource leak
	printf("End of the program\n");

	return 0;
}
