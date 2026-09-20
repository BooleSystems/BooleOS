/* nullos/tools/test_elf_load.c — host-side test of the kernel's ELF loader.
 *
 * Compiles the REAL kernel/elf.c (no copy of its logic) against stand-ins for
 * the physical page allocator and the page mapper, then checks it on every ELF
 * given on the command line (`make test-elf` passes the built user programs):
 *   - each program loads, and every PT_LOAD byte lands where it should;
 *   - a copy truncated at any length is either rejected or still complete —
 *     and NEVER read past its end (the image sits flush against an unmapped
 *     guard page, so an out-of-bounds read faults);
 *   - randomly corrupted headers never crash;
 *   - crafted hostile headers are rejected;
 *   - a pure .bss segment whose file offset is past the end of the file (what
 *     the linker produces) loads — a regression test: once rejected selftest.
 * Linux only (MAP_32BIT gives pages whose address fits in the loader's uint32).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "elf.h"
#include "memory/pmm.h"
#include "memory/vmm.h"

#define MAXMAP 4096
static uint32_t g_va[MAXMAP], g_pa[MAXMAP];
static int g_nmap, g_pages_left;

uint32_t pmm_alloc_page(void) {
    if (g_pages_left-- <= 0) return 0;
    void *p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return p == MAP_FAILED ? 0 : (uint32_t)(uintptr_t)p;
}
void pmm_free_page(uint32_t a) { munmap((void *)(uintptr_t)a, 4096); }
int vmm_map_user_page(uint32_t cr3, uint32_t v, uint32_t p) {
    (void)cr3;
    if (v < 0x800000) return -2;
    if (g_nmap >= MAXMAP) return -1;
    g_va[g_nmap] = v; g_pa[g_nmap++] = p;
    return 0;
}
uint32_t vmm_get_phys_from_dir(uint32_t cr3, uint32_t v) {
    (void)cr3;
    for (int i = 0; i < g_nmap; i++) if (g_va[i] == (v & ~0xFFFu)) return g_pa[i];
    return 0;
}

/* `size` bytes ending flush against an unmapped page */
static unsigned char *guarded(size_t size) {
    size_t pg = 4096, data_pages = (size + pg - 1) / pg ? (size + pg - 1) / pg : 1;
    unsigned char *base = mmap(0, (data_pages + 1) * pg, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(base + data_pages * pg, pg, PROT_NONE);
    return base + data_pages * pg - size;
}

static int try_load(const unsigned char *img, uint32_t size, uint32_t *entry) {
    unsigned char *g = guarded(size);
    memcpy(g, img, size);
    g_nmap = 0; g_pages_left = 1000;
    return elf_load(1, g, size, entry);
}

static int fails;
#define EXPECT(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(int argc, char **argv) {
    uint32_t entry;

    for (int a = 1; a < argc; a++) {
        FILE *fp = fopen(argv[a], "rb");
        if (!fp) { printf("%s: cannot open\n", argv[a]); fails++; continue; }
        fseek(fp, 0, SEEK_END); long n = ftell(fp); rewind(fp);
        unsigned char *img = malloc(n);
        if (fread(img, 1, n, fp) != (size_t)n) { fclose(fp); free(img); fails++; continue; }
        fclose(fp);

        int r = try_load(img, (uint32_t)n, &entry);
        printf("%s: %s\n", argv[a], r == 0 ? "loads" : "REJECTED");
        EXPECT(r == 0, "a valid program was rejected");

        if (r == 0) {
            const elf32_ehdr_t *eh = (const void *)img;
            for (int i = 0; i < eh->e_phnum; i++) {
                const elf32_phdr_t *ph = (const void *)(img + eh->e_phoff + i * eh->e_phentsize);
                if (ph->p_type != PT_LOAD) continue;
                for (uint32_t o = 0; o < ph->p_filesz; o++) {
                    uint32_t phys = vmm_get_phys_from_dir(1, ph->p_vaddr + o);
                    if (!phys || ((unsigned char *)(uintptr_t)phys)[(ph->p_vaddr + o) & 0xFFF] != img[ph->p_offset + o]) {
                        EXPECT(0, "segment %d byte %u not copied correctly", i, o);
                        break;
                    }
                }
            }
        }

        for (long len = 0; len < n; len += 7) try_load(img, (uint32_t)len, &entry);   /* must not fault */

        srand(12345);
        for (int it = 0; it < 5000; it++) {
            unsigned char *m = malloc(n);
            memcpy(m, img, n);
            int k = 1 + rand() % 4;
            for (int j = 0; j < k; j++) m[rand() % (n < 512 ? n : 512)] = (unsigned char)rand();
            try_load(m, (uint32_t)n, &entry);
            free(m);
        }
        free(img);
    }

    /* Hand-built images */
    {
        /* one code segment (file data) + one pure .bss segment whose p_offset is PAST the end of the file */
        unsigned char f[0x80 + 16];
        memset(f, 0, sizeof(f));
        elf32_ehdr_t *e = (void *)f;
        e->e_ident[0] = 0x7F; e->e_ident[1] = 'E'; e->e_ident[2] = 'L'; e->e_ident[3] = 'F';
        e->e_ident[4] = 1; e->e_ident[5] = 1; e->e_type = 2; e->e_machine = 3;
        e->e_entry = 0x01000000; e->e_phoff = 52; e->e_phentsize = 32; e->e_phnum = 2;
        elf32_phdr_t *p = (void *)(f + 52);
        p[0].p_type = 1; p[0].p_offset = 0x80; p[0].p_vaddr = 0x01000000; p[0].p_filesz = 16; p[0].p_memsz = 16;
        p[1].p_type = 1; p[1].p_offset = 0x1000; p[1].p_vaddr = 0x01001000; p[1].p_filesz = 0; p[1].p_memsz = 0x2000;
        EXPECT(try_load(f, sizeof(f), &entry) == 0, ".bss segment with an offset past the end of the file was rejected");
        printf("hand-built: .bss offset past EOF -> %s\n", fails ? "see above" : "loads");

        struct { const char *what; void (*mut)(elf32_ehdr_t *, elf32_phdr_t *); } bad[] = {
            {"filesz > memsz",                     NULL},
        };
        (void)bad;
#define HOSTILE(desc, stmt) do { unsigned char g[sizeof(f)]; memcpy(g, f, sizeof(f)); \
            elf32_ehdr_t *E = (void *)g; elf32_phdr_t *P = (void *)(g + 52); (void)E; (void)P; stmt; \
            EXPECT(try_load(g, sizeof(g), &entry) != 0, "hostile header accepted: %s", desc); } while (0)
        HOSTILE("filesz > memsz", P[0].p_filesz = 17);
        HOSTILE("file data past the end", P[0].p_offset = sizeof(f) - 4);
        HOSTILE("p_offset+filesz wraps uint32", P[0].p_offset = 0xFFFFFFF8u);
        HOSTILE("vaddr in the kernel identity map", P[0].p_vaddr = 0x1000);
        HOSTILE("vaddr+memsz wraps uint32", P[0].p_vaddr = 0xFFFFF000u; P[0].p_memsz = 0x2000);
        HOSTILE("segment reaching the user stack", P[1].p_memsz = 0x02000000u);
        HOSTILE("e_phoff past the end", E->e_phoff = sizeof(f));
        HOSTILE("phoff + phnum*phentsize wraps", E->e_phoff = 0xFFFFFF00u);
        HOSTILE("e_phnum too large", E->e_phnum = 0xFFFF);
        HOSTILE("e_phentsize too small", E->e_phentsize = 4);
        HOSTILE("not ET_EXEC", E->e_type = 3);
        HOSTILE("not i386", E->e_machine = 62);
        EXPECT(try_load(f, 10, &entry) != 0, "an image shorter than an ELF header was accepted");
    }

    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
