#ifndef SHELL_APP_H
#define SHELL_APP_H

#include <stdint.h>
#include <stdbool.h>

#define SHELL_MAX_COMMAND_LENGTH  64
#define SHELL_MAX_ARGS            8
#define SHELL_HISTORY_SIZE        10
#define SHELL_MAX_PATH_LEN        128
#define SHELL_MAX_LINE_LEN        80
#define SHELL_MAX_NAME_LEN        64

typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

typedef struct {
    const char       *name;
    const char       *help;
    shell_cmd_func_t  function;
} shell_command_t;

typedef struct {
    char cmd_buffer[SHELL_MAX_COMMAND_LENGTH];
    int  cmd_len;
    char history[SHELL_HISTORY_SIZE][SHELL_MAX_COMMAND_LENGTH];
    int  history_count;
    int  history_index;
    int  history_pos;
    char current_dir[SHELL_MAX_PATH_LEN];
    bool esc_seq;
    bool esc_bracket;
} shell_state_t;

void           shell_init(void);
void           shell_process(uint8_t *buffer, uint16_t len);
shell_state_t *shell_get_state(void);
int            shell_execute(const char *cmd_line);
void           shell_print(const char *str);
void           shell_printf(const char *fmt, ...);

#endif /* SHELL_APP_H */
