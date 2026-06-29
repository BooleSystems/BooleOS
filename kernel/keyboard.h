// nullos/kernel/keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H
void keyboard_init(void);
char keyboard_getchar(void);
int  keyboard_haschar(void);
int  keyboard_getchar_nowait(void);
int  keyboard_raw_nowait(void);  /* scancode bruto; bit8=ctrl; -1 se vazio */
void keyboard_flush(void);       /* esvazia ambos os buffers */
#endif
