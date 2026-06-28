#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

// Salta para ring 3. Não retorna.
// Antes de chamar: tss_set_stack() deve estar atualizado com o kernel esp0
// do processo atual para que int 0x80 possa retornar ao kernel.
void jump_to_usermode(uint32_t entry, uint32_t user_esp);

#endif
