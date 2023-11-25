#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	char *ptr = malloc(10);
	printf("ptr = %p\n", ptr);
	ptr[0] = 'A';
	free(ptr);

	if (argc == 2) {
		free(ptr);
	} else {
		printf("To test double free, run: %s 2\n", argv[0]);
	}
}
