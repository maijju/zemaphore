#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "common_threads.h"
#include "zemaphore.h"

int max;
volatile int counter = 0; // shared global variable


void *adderA(void *arg) {
    for (int i = 0; i < max; i++) {
	counter = counter + 1; // shared: only one
    }
}

void *adderB(void *arg) {
    for (int i = 0; i < max; i++) {
	counter = counter + 2; // shared: only one
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
	fprintf(stderr, "usage: main-first <loopcount>\n");
	exit(1);
    }
    max = atoi(argv[1]);

    pthread_t p1, p2;

    Pthread_create(&p1, NULL, adderA, "A");
    Pthread_create(&p2, NULL, adderB, "B");

    Pthread_join(p1, NULL);
    Pthread_join(p2, NULL);

    return 0;
}

