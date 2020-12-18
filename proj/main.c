#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
	int fd;
	char *buf1 = "Hello";
	char *buf2;

	printf("Input : %s\n", buf1);

	if((fd = open("/dev/sbulla",O_RDWR)) < 0) {
	    perror("open error");
		exit(1);
	}
	
	if(write(fd, buf1, 5) < 0) {
		perror("write error");
		exit(1);
	}
    
    fsync(fd);
    close(fd);
	if((fd = open("/dev/sbulla",O_RDWR)) < 0) {
	    perror("open error");
		exit(1);
	}
    
	buf2 = (char*)malloc(5);
	
	if(read(fd, buf2, 5) < 0) {
		perror("read error");
		exit(1);
	}
	
	printf("Output : %s\n", buf2);
	
	close(fd);

	return 0;
}
