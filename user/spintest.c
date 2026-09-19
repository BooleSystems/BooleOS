/* nullos/user/spintest.c — tests preemption: never calls yield */
#include "lib/nullos.h"

static const char msg[] = "spintest: still spinning\n";

void _start(void) {
    volatile unsigned int counter = 0;
    for (;;) {
        counter++;
        if (counter % 5000000 == 0)
            nos_write(1, msg, sizeof(msg) - 1);
    }
}
