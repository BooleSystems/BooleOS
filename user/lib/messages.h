/* nullos/user/lib/messages.h — central table of user-visible userland text.
 *
 * The userland counterpart of kernel/messages.h (user programs cannot call
 * the kernel's msg(): separate address space, no shared code). Same idea:
 * every string a program prints for a human to read is fetched by ID,
 * console output being `nos_write(1, msg(UMSG_X), ...)`. NOT a translation
 * system — one English column.
 *
 * Differences from the kernel table, on purpose: enum type umsg_id_t and IDs
 * prefixed UMSG_<PROGRAM>_ (never MSG_), so the two headers cannot be mixed
 * up. The function is also called msg(), so including both headers in one
 * file is a compile error (conflicting declarations), not a silent mixup.
 *
 * Rules (same as the kernel): only OUTPUT text goes here — never a shell
 * command name compared with strcmp, a program name given to nos_exec(), or
 * a file name; strings are FRAGMENTS (numbers stay nos_uitoa() between
 * them); IDs are never reused for a different string. Whitespace-only
 * layout strings, the version banner lines and single characters assembled
 * one by one (edit.c's status bar) stay literals at their use.
 * selftest.c and forktest.c are diagnostic output and are not migrated;
 * init.c and spintest.c print one demo line each and are not linked with
 * this table.
 */

#ifndef NOS_MESSAGES_H
#define NOS_MESSAGES_H

typedef enum {
    /* shell (shell.c) */
    UMSG_SH_LOGO_1,
    UMSG_SH_LOGO_2,
    UMSG_SH_LOGO_3,
    UMSG_SH_LOGO_4,
    UMSG_SH_LOGO_5,
    UMSG_SH_FETCH_ARCH,
    UMSG_SH_FETCH_UPTIME,
    UMSG_SH_FETCH_SECONDS_NL,
    UMSG_SH_FETCH_MEM_PMM,
    UMSG_SH_FETCH_KB_FREE_NL,
    UMSG_SH_FETCH_HEAP,
    UMSG_SH_FETCH_B_FREE_NL,
    UMSG_SH_FETCH_PROCS,
    UMSG_SH_FETCH_RUNNING_NL,
    UMSG_SH_MEM_PMM_LINE,
    UMSG_SH_MEM_FREE_PAGES,
    UMSG_SH_MEM_KB_CLOSE_NL,
    UMSG_SH_MEM_HEAP_LINE,
    UMSG_SH_USAGE_KILL_PID,
    UMSG_SH_INVALID_PID,
    UMSG_SH_SHUTTING_DOWN_SHELL,
    UMSG_SH_KILL_PROCESS,
    UMSG_SH_KILL_TERMINATED_NL,
    UMSG_SH_PID_NOT_FOUND,
    UMSG_SH_USAGE_TOUCH_FILE,
    UMSG_SH_TOUCH_CANNOT_CREATE,
    UMSG_SH_USAGE_MKDIR_DIR,
    UMSG_SH_MKDIR_CANNOT_CREATE,
    UMSG_SH_CD_NO_SUCH_DIRECTORY,
    UMSG_SH_PWD_CANNOT_DETERMINE,
    UMSG_SH_PIPE_COULD_NOT_CREATE_PIPE,
    UMSG_SH_PIPE_PROGRAM_NOT_FOUND,
    UMSG_SH_USAGE_CMD_INFILE_OUTFILE,
    UMSG_SH_REDIRECT_NO_ARGS,
    UMSG_SH_REDIRECT_CANNOT_OPEN,
    UMSG_SH_REDIRECT_CANNOT_WRITE_TO,
    UMSG_SH_REDIRECT_NOT_FAT16_NL,
    UMSG_SH_REDIRECT_PROGRAM_NOT_FOUND,
    UMSG_SH_REDIRECT_BUILTIN_NL,
    UMSG_SH_USAGE_RUN_PROGRAM,
    UMSG_SH_RUN_NOT_FOUND,
    UMSG_SH_RUN_RUNNING,
    UMSG_SH_BYE,
    UMSG_SH_COMMAND_NOT_FOUND,
    UMSG_SH_WELCOME,
    UMSG_SH_PROMPT,
    UMSG_SH_REDIRECT_WITH_PIPE,
    UMSG_SH_USAGE_CMD1_CMD2,
    UMSG_SH_USAGE_CAT_FILE,
    UMSG_SH_ERROR_CAT_NOT_FOUND,
    UMSG_SH_ERROR_EDIT_NOT_FOUND,
    UMSG_SH_HELP_TEXT,

    /* editor (edit.c) */
    UMSG_ED_HELP_BAR,
    UMSG_ED_NO_NAME,
    UMSG_ED_SAVED,
    UMSG_ED_SAVED_NO_DISK,

    /* cat (cat.c) */
    UMSG_CAT_CANNOT_OPEN,

    UMSG_COUNT
} umsg_id_t;

/* Text for id, or "(?)" if id is out of range or has no text. Never NULL. */
const char *msg(umsg_id_t id);

#endif /* NOS_MESSAGES_H */
