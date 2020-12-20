#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define BUF_SIZE 4096

int main() {
	int fd;
	char *buf1, *buf2;
	char comp_decomp_succeed;
	buf1 = (char *)malloc(4096);
	buf2 = (char *)malloc(4096);
	char letters[26] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};	

	for (int i=0;i<100;i++) {
		comp_decomp_succeed = 1;
		srand((unsigned int)i);
		for (int j=0;j<4096;j++) {
			//srand((unsigned int)time(NULL));
			buf1[j] = letters[rand()%26];
		}
		if ((fd = open("/dev/sbulla", O_RDWR)) < 0) {
			perror("open error");
			exit(1);
		}
		if (write(fd, buf1, 4096) < 0) {
			perror("write error");
			exit(1);
		}
		fsync(fd);
		close(fd);
		if ((fd = open("/dev/sbulla", O_RDWR)) < 0) {
			perror("open error");
			exit(1);
		}
		if (read(fd, buf2, 4096) < 0) {
			perror("read error");
			exit(1);
		}
		for (int j=0;j<4096;j++) {
			if (buf1[j] != buf2[j]) {
				comp_decomp_succeed = 0;
				break;
			}
		}
		fsync(fd);
		close(fd);
		if (comp_decomp_succeed) printf("succeed\n");
		else printf("failed\n");
	}
	return 0;
}
