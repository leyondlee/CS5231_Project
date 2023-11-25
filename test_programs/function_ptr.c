#include <stdio.h>
#include <stdlib.h>

void func1() {
	printf("In function 1\n");
}

void func2() {
	printf("In function 2\n");
}

void funcDefault() {
	printf("In default function\n");
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s <1,2,3>\n", argv[0]);
		return 1;
	}

	char *endptr;
	unsigned long input = strtoul(argv[1], &endptr, 10);

	void (*func)();
	switch (input) {
	case 1:
		func = &func1;
		break;

	case 2:
		func = &func2;
		break;

	default:
		func = &funcDefault;
	}

	(*func)();
}
