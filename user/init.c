/* nullos/user/init.c — first user process */
#include "lib/nullos.h"

static const char msg[] = "init: hello from userland!\n";

void _start(void) {
    nos_write(1, msg, sizeof(msg) - 1);
    nos_exit(0);
}
