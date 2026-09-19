/* nullos/kernel/pipe.c — in-kernel pipe buffers (Phase 16). See
   docs/pipes.md for the full design writeup this file implements:
   fixed static pool, single-waiter-per-direction, symmetric
   EOF/broken-pipe wakeups. The blocking pattern mirrors
   kernel/drivers/ata.c's ata_wait_irq()/g_irq_waiter exactly: the
   "check the condition, and if not satisfied, register as the
   waiter and block" sequence happens inside one cli/sti section, so
   a wakeup that becomes available between the check and the block
   can never be lost. */
#include "pipe.h"
#include "process.h"
#include "scheduler.h"
#include <stdint.h>

typedef struct {
    uint8_t    buf[PIPE_BUF_SIZE];
    uint32_t   head, tail, count;
    uint8_t    used;
    uint32_t   read_refs, write_refs;
    process_t *read_waiter;
    process_t *write_waiter;
} pipe_t;

static pipe_t pipe_table[PIPE_MAX];

int pipe_create(uint32_t *out_pipe_idx) {
    for (uint32_t i = 0; i < PIPE_MAX; i++) {
        pipe_t *p = &pipe_table[i];
        if (p->used) continue;

        p->head = p->tail = p->count = 0;
        p->used = 1;
        p->read_refs = 1;
        p->write_refs = 1;
        p->read_waiter = 0;
        p->write_waiter = 0;

        if (out_pipe_idx) *out_pipe_idx = i;
        return 0;
    }
    return -1;  /* pool exhausted */
}

int pipe_read(uint32_t pipe_idx, char *buf, uint32_t len) {
    if (pipe_idx >= PIPE_MAX || !buf) return -1;
    pipe_t *p = &pipe_table[pipe_idx];

    for (;;) {
        __asm__ volatile ("cli");

        if (p->count > 0) {
            uint32_t read = 0;
            while (read < len && p->count > 0) {
                buf[read++] = (char)p->buf[p->tail];
                p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
                p->count--;
            }
            /* freed space: wake a writer blocked on a full buffer */
            process_t *w = p->write_waiter;
            if (w) {
                p->write_waiter = 0;
                if (w->state == PROCESS_BLOCKED) w->state = PROCESS_READY;
            }
            __asm__ volatile ("sti");
            return (int)read;
        }

        if (p->write_refs == 0) {
            __asm__ volatile ("sti");
            return 0;   /* EOF: empty, and no writer left to fill it */
        }

        process_t *self = process_current();
        if (!self) { __asm__ volatile ("sti"); return 0; }

        p->read_waiter = self;
        self->state = PROCESS_BLOCKED;
        __asm__ volatile ("sti");

        scheduler_block_current();   /* resumes once a writer or pipe_release_write() wakes us */
    }
}

int pipe_write(uint32_t pipe_idx, const char *buf, uint32_t len) {
    if (pipe_idx >= PIPE_MAX || !buf) return -1;
    pipe_t *p = &pipe_table[pipe_idx];

    uint32_t written = 0;
    for (;;) {
        __asm__ volatile ("cli");

        if (p->read_refs == 0) {
            __asm__ volatile ("sti");
            return -1;   /* broken pipe: no reader left, even mid-write */
        }

        uint32_t free_space = PIPE_BUF_SIZE - p->count;
        if (free_space > 0) {
            uint32_t remaining = len - written;
            uint32_t to_write = (remaining < free_space) ? remaining : free_space;
            for (uint32_t i = 0; i < to_write; i++) {
                p->buf[p->head] = (uint8_t)buf[written + i];
                p->head = (p->head + 1) % PIPE_BUF_SIZE;
                p->count++;
            }
            written += to_write;

            /* added data: wake a reader blocked on an empty buffer */
            process_t *r = p->read_waiter;
            if (r) {
                p->read_waiter = 0;
                if (r->state == PROCESS_BLOCKED) r->state = PROCESS_READY;
            }

            if (written == len) { __asm__ volatile ("sti"); return 0; }
            __asm__ volatile ("sti");
            continue;   /* buffer is full again (or read_refs dropped) — recheck at the top */
        }

        process_t *self = process_current();
        if (!self) { __asm__ volatile ("sti"); return -1; }

        p->write_waiter = self;
        self->state = PROCESS_BLOCKED;
        __asm__ volatile ("sti");

        scheduler_block_current();   /* resumes once a reader or pipe_release_read() wakes us */
    }
}

void pipe_add_read_ref(uint32_t pipe_idx) {
    if (pipe_idx >= PIPE_MAX) return;
    __asm__ volatile ("cli");
    pipe_table[pipe_idx].read_refs++;
    __asm__ volatile ("sti");
}

void pipe_add_write_ref(uint32_t pipe_idx) {
    if (pipe_idx >= PIPE_MAX) return;
    __asm__ volatile ("cli");
    pipe_table[pipe_idx].write_refs++;
    __asm__ volatile ("sti");
}

void pipe_release_read(uint32_t pipe_idx) {
    if (pipe_idx >= PIPE_MAX) return;
    pipe_t *p = &pipe_table[pipe_idx];

    __asm__ volatile ("cli");
    if (p->read_refs > 0) p->read_refs--;

    if (p->read_refs == 0) {
        /* last reader gone: wake a blocked writer into broken-pipe */
        process_t *w = p->write_waiter;
        if (w) {
            p->write_waiter = 0;
            if (w->state == PROCESS_BLOCKED) w->state = PROCESS_READY;
        }
    }
    if (p->read_refs == 0 && p->write_refs == 0) p->used = 0;
    __asm__ volatile ("sti");
}

void pipe_release_write(uint32_t pipe_idx) {
    if (pipe_idx >= PIPE_MAX) return;
    pipe_t *p = &pipe_table[pipe_idx];

    __asm__ volatile ("cli");
    if (p->write_refs > 0) p->write_refs--;

    if (p->write_refs == 0) {
        /* last writer gone: wake a blocked reader into EOF */
        process_t *r = p->read_waiter;
        if (r) {
            p->read_waiter = 0;
            if (r->state == PROCESS_BLOCKED) r->state = PROCESS_READY;
        }
    }
    if (p->read_refs == 0 && p->write_refs == 0) p->used = 0;
    __asm__ volatile ("sti");
}
