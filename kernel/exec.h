#ifndef EXEC_H
#define EXEC_H

#include "process.h"

/*
 * exec - load a program from the ramfs and spawn it as a user process.
 *
 * name : filename to look up in the ramfs (e.g. "init")
 *
 * Returns a pointer to the new process on success, NULL on failure.
 * ramfs_init() must have been called before exec().
 */
process_t *exec(const char *name);

#endif
