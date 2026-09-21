/* nullos/user/lib/messages.c — the userland message table (see messages.h). */

#include "messages.h"

#define UMSG_FALLBACK "(?)"

static const char *const g_umsgs[] = {
    /* shell (shell.c) */
    [UMSG_SH_LOGO_1] = "  _   _       _ _  ___  ____  ",
    [UMSG_SH_LOGO_2] = " | \\ | |_   _| | |/ _ \\/ ___| ",
    [UMSG_SH_LOGO_3] = " |  \\| | | | | | | | | \\___ \\ ",
    [UMSG_SH_LOGO_4] = " | |\\  | |_| | | | |_| |___) |",
    [UMSG_SH_LOGO_5] = " |_| \\_|\\__,_|_|_|\\___/|____/ ",
    [UMSG_SH_FETCH_ARCH] = "  Arch: i686\n",
    [UMSG_SH_FETCH_UPTIME] = "  Uptime: ",
    [UMSG_SH_FETCH_SECONDS_NL] = "s\n",
    [UMSG_SH_FETCH_MEM_PMM] = "  Mem PMM: ",
    [UMSG_SH_FETCH_KB_FREE_NL] = " KB free\n",
    [UMSG_SH_FETCH_HEAP] = "  Heap: ",
    [UMSG_SH_FETCH_B_FREE_NL] = " B free\n",
    [UMSG_SH_FETCH_PROCS] = "                                   Procs: ",
    [UMSG_SH_FETCH_RUNNING_NL] = " running\n",
    [UMSG_SH_MEM_PMM_LINE] = "PMM:  ",
    [UMSG_SH_MEM_FREE_PAGES] = " free pages (",
    [UMSG_SH_MEM_KB_CLOSE_NL] = " KB)\n",
    [UMSG_SH_MEM_HEAP_LINE] = "Heap: ",
    [UMSG_SH_USAGE_KILL_PID] = "usage: kill <pid>\n",
    [UMSG_SH_INVALID_PID] = "invalid pid\n",
    [UMSG_SH_SHUTTING_DOWN_SHELL] = "shutting down shell...\n",
    [UMSG_SH_KILL_PROCESS] = "process ",
    [UMSG_SH_KILL_TERMINATED_NL] = " terminated\n",
    [UMSG_SH_PID_NOT_FOUND] = "pid not found\n",
    [UMSG_SH_USAGE_TOUCH_FILE] = "usage: touch <file>\n",
    [UMSG_SH_TOUCH_CANNOT_CREATE] = "error: could not create (no disk?)\n",
    [UMSG_SH_USAGE_MKDIR_DIR] = "usage: mkdir <dir>\n",
    [UMSG_SH_MKDIR_CANNOT_CREATE] = "error: could not create directory (no disk, path missing, or name taken by a file)\n",
    [UMSG_SH_CD_NO_SUCH_DIRECTORY] = "cd: no such directory: ",
    [UMSG_SH_PWD_CANNOT_DETERMINE] = "pwd: cannot determine the current directory\n",
    [UMSG_SH_PIPE_COULD_NOT_CREATE_PIPE] = "pipe: could not create pipe\n",
    [UMSG_SH_PIPE_PROGRAM_NOT_FOUND] = "pipe: program not found: ",
    [UMSG_SH_USAGE_CMD_INFILE_OUTFILE] = "usage: cmd [< infile] [> outfile]\n",
    [UMSG_SH_REDIRECT_NO_ARGS] = "redirect: the program is launched by name only, no arguments\n",
    [UMSG_SH_REDIRECT_CANNOT_OPEN] = "redirect: cannot open: ",
    [UMSG_SH_REDIRECT_CANNOT_WRITE_TO] = "redirect: cannot write to: ",
    [UMSG_SH_REDIRECT_NOT_FAT16_NL] = " (no disk, or not a FAT16 file)\n",
    [UMSG_SH_REDIRECT_PROGRAM_NOT_FOUND] = "redirect: program not found: ",
    [UMSG_SH_REDIRECT_BUILTIN_NL] = " (built-in commands can't be redirected)\n",
    [UMSG_SH_USAGE_RUN_PROGRAM] = "usage: run <program>\n",
    [UMSG_SH_RUN_NOT_FOUND] = "error: program not found\n",
    [UMSG_SH_RUN_RUNNING] = "running: ",
    [UMSG_SH_BYE] = "bye!\n",
    [UMSG_SH_COMMAND_NOT_FOUND] = "command not found: ",
    [UMSG_SH_WELCOME] = "NullOS shell — type 'help'\n",
    [UMSG_SH_PROMPT] = "> ",
    [UMSG_SH_REDIRECT_WITH_PIPE] = "redirection can't be combined with a pipe yet\n",
    [UMSG_SH_USAGE_CMD1_CMD2] = "usage: cmd1 | cmd2\n",
    [UMSG_SH_USAGE_CAT_FILE] = "usage: cat <file>\n",
    [UMSG_SH_ERROR_CAT_NOT_FOUND] = "error: cat not found\n",
    [UMSG_SH_ERROR_EDIT_NOT_FOUND] = "error: edit not found\n",
    [UMSG_SH_HELP_TEXT] = "commands:\n  help           this message\n  uname          system version\n  fetch          system info\n  ps             process table\n  mem            memory usage\n  ls [dir]       list files (cwd, or a given path)\n  lspci          list PCI devices\n  touch <name>   create an empty file (path allowed, e.g. docs/a.txt)\n  mkdir <dir>    create a directory (path allowed)\n  cd [dir]       change the current directory (no arg = root)\n  pwd            print the current directory\n  echo <text>    print text\n  kill <pid>     terminate a process\n  run <prog>     run a program in the background\n  edit <file>    open the text editor\n  cat <file>     print a file\n  cmd < in       run a program with stdin read from a file\n  cmd > out      run a program with stdout written to a file (truncates;\n                 program name only, no arguments, no builtins)\n  reboot         restart the machine\n  shutdown       power the machine off\n  crash <de|pf|gpf>  DEBUG tool: fault on purpose (tests the crash handler)\n  cmd1 | cmd2    pipe cmd1's stdout into cmd2's stdin (both must\n                 be programs, not builtins — e.g. \"forktest | cat\")\n  clear          clear the screen\n  exit           exit the shell\n",
    [UMSG_SH_CRASH_USAGE] = "usage: crash <de|pf|gpf>   (debug tool: makes this program fault on purpose;\nthe machine crashes, saves the crash and restarts into Safe Mode)\n",
    [UMSG_SH_CRASH_DE] = "crash: dividing by zero (#DE) on purpose...\n",
    [UMSG_SH_CRASH_PF] = "crash: reading the invalid address 0xDEADBEEF (#PF) on purpose...\n",
    [UMSG_SH_CRASH_GP] = "crash: loading an invalid segment selector (#GP) on purpose...\n",

    /* editor (edit.c) */
    [UMSG_ED_HELP_BAR] = "^S save  ^Q quit  Arrows: navigate",
    [UMSG_ED_NO_NAME] = "[no name]",
    [UMSG_ED_SAVED] = "saved",
    [UMSG_ED_SAVED_NO_DISK] = "saved (no disk)",

    /* cat (cat.c) */
    [UMSG_CAT_CANNOT_OPEN] = "cat: cannot open ",

};

/* Compile-time check: the table must reach exactly UMSG_COUNT entries (its
   size is set by the highest designated initializer). Fails to compile
   (negative array size) if an ID was added at the end of the enum without a
   text. C99 has no _Static_assert. A gap in the middle is caught at run time
   by the "(?)" fallback. */
typedef char umsg_table_size_check[
    (sizeof(g_umsgs) / sizeof(g_umsgs[0]) == UMSG_COUNT) ? 1 : -1];

const char *msg(umsg_id_t id) {
    if ((unsigned)id >= (unsigned)UMSG_COUNT) return UMSG_FALLBACK;
    const char *s = g_umsgs[id];
    return s ? s : UMSG_FALLBACK;
}
