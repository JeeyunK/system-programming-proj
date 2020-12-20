#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
	int fd;
	char *buf1[10] ={"hello", "my name is jeeyun", "nice to meet you",
 "suppose that a connected planar graph has eight vertices, each of degree tree. Into how many regions is the plane divided by a planar representation of this graph?",
 "10, 34, 59", "6", "this is test samples", "Decemper", "9", "10"};
	//char *buf1 = "Hello";
	//int size[10] = {5, 17, 16, 165, 10, 1, 20, 8, 1, 2};
	int size[10] = {6, 18, 17, 166, 11, 2, 21, 9, 2, 3};
	char *buf2;
	for (int i=0; i<10; i++) {
	printf("Input : %s\n", buf1[i]);

	if((fd = open("/dev/sbulla",O_RDWR)) < 0) {
	    perror("open error");
		exit(1);
	}
	
	if(write(fd, buf1[i], size[i]) < 0) {
		perror("write error");
		exit(1);
	}
    
    fsync(fd);
    close(fd);
	if((fd = open("/dev/sbulla",O_RDWR)) < 0) {
	    perror("open error");
		exit(1);
	}
    
	buf2 = (char*)malloc(size[i]);
	
	if(read(fd, buf2, size[i]) < 0) {
		perror("read error");
		exit(1);
	}
	
	printf("Output : %s\n", buf2);
	free(buf2);
	fsync(fd);
	close(fd);
	}
	return 0;
}
