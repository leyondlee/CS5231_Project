#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s <buffer value>\n", argv[0]);
		return 1;
	}

	char buf[10];
	strcpy(buf, argv[1]);

	printf("buf: %s\n", buf);
}
