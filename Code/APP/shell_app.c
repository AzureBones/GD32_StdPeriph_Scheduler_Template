#include "mcu_cmic_gd32f470vet6.h"
#include "shell_app.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* 内部状态                                                             */
/* ------------------------------------------------------------------ */
static shell_state_t shell_state;

#define SHELL_LFN_SIZE  64

/* 内部路径 "/" → FATFS "0:/" */
static const char *fatfs_path(const char *path)
{
    static char fp[SHELL_MAX_PATH_LEN + 3];
    snprintf(fp, sizeof(fp), "0:%s", path);
    return fp;
}

/* 优先返回长文件名 */
static const char *get_fname(const FILINFO *fno)
{
    return (fno->lfname && fno->lfname[0]) ? fno->lfname : fno->fname;
}

/* ------------------------------------------------------------------ */
/* 命令函数前向声明                                                     */
/* ------------------------------------------------------------------ */
static int cmd_help (int argc, char *argv[]);
static int cmd_ls   (int argc, char *argv[]);
static int cmd_cd   (int argc, char *argv[]);
static int cmd_pwd  (int argc, char *argv[]);
static int cmd_cat  (int argc, char *argv[]);
static int cmd_mkdir(int argc, char *argv[]);
static int cmd_rm   (int argc, char *argv[]);
static int cmd_touch(int argc, char *argv[]);
static int cmd_mv   (int argc, char *argv[]);
static int cmd_cp   (int argc, char *argv[]);
static int cmd_echo (int argc, char *argv[]);
static int cmd_clear(int argc, char *argv[]);
static int cmd_write(int argc, char *argv[]);

static const shell_command_t commands[] = {
    {"help",  "Display help information",  cmd_help },
    {"ls",    "List directory contents",   cmd_ls   },
    {"cd",    "Change current directory",  cmd_cd   },
    {"pwd",   "Print working directory",   cmd_pwd  },
    {"cat",   "Display file contents",     cmd_cat  },
    {"mkdir", "Create directory",          cmd_mkdir},
    {"rm",    "Remove file or directory",  cmd_rm   },
    {"touch", "Create empty file",         cmd_touch},
    {"mv",    "Move/rename file",          cmd_mv   },
    {"cp",    "Copy file",                 cmd_cp   },
    {"echo",  "Display text",              cmd_echo },
    {"clear", "Clear screen",              cmd_clear},
    {"write", "Write text to file",        cmd_write},
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* ------------------------------------------------------------------ */
/* 内部函数前向声明                                                     */
/* ------------------------------------------------------------------ */
static void  shell_process_char(uint8_t ch);
static void  shell_execute_line(void);
static void  shell_handle_backspace(void);
static void  shell_handle_tab(void);
static void  shell_handle_escape_sequence(uint8_t ch);
static void  shell_handle_history(int direction);
static void  shell_add_to_history(const char *cmd);
static int   shell_parse_args(char *cmd_line, char *argv[]);
static void  shell_prompt(void);
static void  shell_clear_line(void);
static char *shell_normalize_path(const char *path);
static int   shell_is_dir(const char *path);

/* ------------------------------------------------------------------ */
/* 公共接口                                                             */
/* ------------------------------------------------------------------ */
void shell_init(void)
{
    memset(&shell_state, 0, sizeof(shell_state));
    strcpy(shell_state.current_dir, "/");
    shell_printf("\r\n== FATFS Shell v1.0 ==\r\n");
    shell_printf("Type 'help' for available commands.\r\n");
    shell_prompt();
}

shell_state_t *shell_get_state(void)
{
    return &shell_state;
}

void shell_process(uint8_t *buffer, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        shell_process_char(buffer[i]);
    }
}

void shell_print(const char *str)
{
    my_printf(DEBUG_USART, "%s", str);
}

void shell_printf(const char *fmt, ...)
{
    char    buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    shell_print(buf);
}

/* ------------------------------------------------------------------ */
/* 字符处理                                                             */
/* ------------------------------------------------------------------ */
static void shell_process_char(uint8_t ch)
{
    if (shell_state.esc_seq) {
        shell_handle_escape_sequence(ch);
        return;
    }
    if (ch == 27) { shell_state.esc_seq = true; return; }

    if (ch == '\r' || ch == '\n') {
        shell_printf("\r\n");
        if (shell_state.cmd_len > 0) shell_execute_line();
        shell_prompt();
        return;
    }
    if (ch == '\b' || ch == 127) { shell_handle_backspace(); return; }
    if (ch == '\t')               { shell_handle_tab();       return; }

    if (ch >= 32 && ch < 127 && shell_state.cmd_len < SHELL_MAX_COMMAND_LENGTH - 1) {
        shell_state.cmd_buffer[shell_state.cmd_len++] = ch;
        shell_state.cmd_buffer[shell_state.cmd_len]   = '\0';
        shell_printf("%c", ch);
    }
}

static void shell_handle_escape_sequence(uint8_t ch)
{
    if (shell_state.esc_bracket) {
        if      (ch == 'A') shell_handle_history(-1);
        else if (ch == 'B') shell_handle_history(1);
        shell_state.esc_seq     = false;
        shell_state.esc_bracket = false;
        return;
    }
    if (ch == '[') { shell_state.esc_bracket = true; return; }
    shell_state.esc_seq     = false;
    shell_state.esc_bracket = false;
}

static void shell_execute_line(void)
{
    shell_add_to_history(shell_state.cmd_buffer);
    shell_execute(shell_state.cmd_buffer);
    shell_state.cmd_buffer[0] = '\0';
    shell_state.cmd_len       = 0;
    shell_state.history_pos   = -1;
}

int shell_execute(const char *cmd_line)
{
    char  cmd_copy[SHELL_MAX_COMMAND_LENGTH];
    char *argv[SHELL_MAX_ARGS];

    strncpy(cmd_copy, cmd_line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    int argc = shell_parse_args(cmd_copy, argv);
    if (argc == 0) return 0;

    for (int i = 0; i < (int)NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0)
            return commands[i].function(argc, argv);
    }
    shell_printf("Unknown command: %s\r\n", argv[0]);
    return -1;
}

static int shell_parse_args(char *cmd_line, char *argv[])
{
    int   argc  = 0;
    char *token = strtok(cmd_line, " \t");
    while (token && argc < SHELL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    return argc;
}

static void shell_handle_backspace(void)
{
    if (shell_state.cmd_len > 0) {
        shell_state.cmd_len--;
        shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
        shell_printf("\b \b");
    }
}

static void shell_handle_tab(void)
{
    char *input     = shell_state.cmd_buffer;
    int   input_len = shell_state.cmd_len;

    int cmd_start = 0;
    while (cmd_start < input_len && input[cmd_start] == ' ') cmd_start++;
    int cmd_end = cmd_start;
    while (cmd_end < input_len && input[cmd_end] != ' ')    cmd_end++;
    int param_start = cmd_end;
    while (param_start < input_len && input[param_start] == ' ') param_start++;

    char cmd_name[SHELL_MAX_COMMAND_LENGTH] = {0};
    if (cmd_end > cmd_start)
        strncpy(cmd_name, input + cmd_start, (size_t)(cmd_end - cmd_start));

    /* 命令名补全 */
    if (param_start >= input_len) {
        const char *prefix     = input + cmd_start;
        int         prefix_len = input_len - cmd_start;
        int matches = 0, match_idx = -1;
        for (int i = 0; i < (int)NUM_COMMANDS; i++) {
            if (strncmp(prefix, commands[i].name, (size_t)prefix_len) == 0) {
                matches++; match_idx = i;
            }
        }
        if (matches == 1) {
            strcpy(shell_state.cmd_buffer, commands[match_idx].name);
            shell_state.cmd_len = (int)strlen(shell_state.cmd_buffer);
            shell_state.cmd_buffer[shell_state.cmd_len++] = ' ';
            shell_state.cmd_buffer[shell_state.cmd_len]   = '\0';
            shell_clear_line();
            shell_printf("> %s", shell_state.cmd_buffer);
        } else if (matches > 1) {
            shell_printf("\r\nPossible commands:\r\n");
            for (int i = 0; i < (int)NUM_COMMANDS; i++) {
                if (strncmp(prefix, commands[i].name, (size_t)prefix_len) == 0)
                    shell_printf("  %s\r\n", commands[i].name);
            }
            shell_prompt();
            shell_printf("%s", shell_state.cmd_buffer);
        }
        return;
    }

    /* 文件路径补全 */
    int requires_path = 0, dir_only = 0;
    if (strcmp(cmd_name, "cd") == 0 || strcmp(cmd_name, "mkdir") == 0)
        { requires_path = 1; dir_only = 1; }
    else if (strcmp(cmd_name, "cat")   == 0 || strcmp(cmd_name, "rm")    == 0 ||
             strcmp(cmd_name, "touch") == 0 || strcmp(cmd_name, "write") == 0 ||
             strcmp(cmd_name, "mv")    == 0 || strcmp(cmd_name, "cp")    == 0)
        requires_path = 1;

    if (!requires_path) return;

    char partial_path[SHELL_MAX_PATH_LEN] = {0};
    strncpy(partial_path, input + param_start, (size_t)(input_len - param_start));

    char  dir_part[SHELL_MAX_PATH_LEN]  = {0};
    char  file_part[SHELL_MAX_PATH_LEN] = {0};
    char *last_slash = strrchr(partial_path, '/');
    if (last_slash) {
        strncpy(dir_part, partial_path, (size_t)(last_slash - partial_path + 1));
        strcpy(file_part, last_slash + 1);
    } else {
        strcpy(dir_part,  "./");
        strcpy(file_part, partial_path);
    }

    char *full_dir = (dir_part[0] == '\0') ? shell_normalize_path(".")
                                            : shell_normalize_path(dir_part);

    DIR     dir;
    FILINFO info;
    char    lfn[SHELL_LFN_SIZE];
    info.lfname = lfn;
    info.lfsize = sizeof(lfn);

    if (f_opendir(&dir, fatfs_path(full_dir)) != FR_OK) return;

    struct { char name[SHELL_MAX_NAME_LEN]; int is_dir; } matches[32];
    int match_count   = 0;
    int file_part_len = (int)strlen(file_part);

    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] && match_count < 32) {
        const char *name     = get_fname(&info);
        int         is_dir_e = (info.fattrib & AM_DIR) != 0;
        if (strncmp(file_part, name, (size_t)file_part_len) != 0) continue;
        if (dir_only && !is_dir_e) continue;
        strncpy(matches[match_count].name, name, SHELL_MAX_NAME_LEN - 1);
        matches[match_count].name[SHELL_MAX_NAME_LEN - 1] = '\0';
        matches[match_count].is_dir = is_dir_e;
        match_count++;
    }
    if (match_count == 0) return;
    if (match_count == 1) {
        char new_cmd[SHELL_MAX_COMMAND_LENGTH] = {0};
        strncpy(new_cmd, input, (size_t)param_start);
        if (last_slash)
            strncat(new_cmd, partial_path, (size_t)(last_slash - partial_path + 1));
        strncat(new_cmd, matches[0].name,
                sizeof(new_cmd) - strlen(new_cmd) - 1);
        strncat(new_cmd, matches[0].is_dir ? "/" : " ",
                sizeof(new_cmd) - strlen(new_cmd) - 1);
        strcpy(shell_state.cmd_buffer, new_cmd);
        shell_state.cmd_len = (int)strlen(new_cmd);
        shell_clear_line();
        shell_printf("> %s", shell_state.cmd_buffer);
    } else {
        shell_printf("\r\nPossible completions:\r\n");
        for (int i = 0; i < match_count; i++) {
            shell_printf(matches[i].is_dir ? "  [DIR]  %s/\r\n"
                                           : "  [FILE] %s\r\n",
                         matches[i].name);
        }
        shell_prompt();
        shell_printf("%s", shell_state.cmd_buffer);
    }
}

static void shell_handle_history(int direction)
{
    if (shell_state.history_count == 0) return;
    if (shell_state.history_pos == -1) shell_state.history_pos = 0;
    else                               shell_state.history_pos += direction;

    if (shell_state.history_pos < 0)
        shell_state.history_pos = -1;
    else if (shell_state.history_pos >= shell_state.history_count)
        shell_state.history_pos = shell_state.history_count - 1;

    shell_clear_line();
    if (shell_state.history_pos == -1) {
        shell_state.cmd_buffer[0] = '\0';
        shell_state.cmd_len       = 0;
    } else {
        int idx = (shell_state.history_index - 1 - shell_state.history_pos
                   + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
        strcpy(shell_state.cmd_buffer, shell_state.history[idx]);
        shell_state.cmd_len = (int)strlen(shell_state.cmd_buffer);
    }
    shell_printf("> %s", shell_state.cmd_buffer);
}

static void shell_add_to_history(const char *cmd)
{
    if (strlen(cmd) == 0) return;
    if (shell_state.history_count > 0) {
        int last = (shell_state.history_index - 1 + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
        if (strcmp(shell_state.history[last], cmd) == 0) return;
    }
    strncpy(shell_state.history[shell_state.history_index], cmd,
            SHELL_MAX_COMMAND_LENGTH - 1);
    shell_state.history[shell_state.history_index][SHELL_MAX_COMMAND_LENGTH - 1] = '\0';
    shell_state.history_index = (shell_state.history_index + 1) % SHELL_HISTORY_SIZE;
    if (shell_state.history_count < SHELL_HISTORY_SIZE) shell_state.history_count++;
}

static void shell_prompt(void)     { shell_printf("> "); }
static void shell_clear_line(void) { shell_printf("\r\033[K"); }

/* ------------------------------------------------------------------ */
/* 路径工具                                                             */
/* ------------------------------------------------------------------ */
static char *shell_normalize_path(const char *path)
{
    static char  norm_path[SHELL_MAX_PATH_LEN];
    char        *parts[SHELL_MAX_PATH_LEN / 2];
    int          num_parts = 0;
    char         path_copy[SHELL_MAX_PATH_LEN];

    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *curr_path;
    if (path_copy[0] == '/') {
        curr_path = path_copy + 1;
        parts[num_parts++] = "/";
    } else {
        curr_path = path_copy;
        if (strcmp(shell_state.current_dir, "/") == 0) {
            parts[num_parts++] = "/";
        } else {
            char curr_copy[SHELL_MAX_PATH_LEN];
            strncpy(curr_copy, shell_state.current_dir, sizeof(curr_copy));
            char *dp = strtok(curr_copy, "/");
            while (dp && num_parts < SHELL_MAX_PATH_LEN / 2 - 1) {
                parts[num_parts++] = dp;
                dp = strtok(NULL, "/");
            }
        }
    }

    char *part = strtok(curr_path, "/");
    while (part && num_parts < SHELL_MAX_PATH_LEN / 2 - 1) {
        if (strcmp(part, ".") == 0) {
            /* skip */
        } else if (strcmp(part, "..") == 0) {
            if (num_parts > 1 || strcmp(parts[0], "/") != 0) num_parts--;
        } else {
            parts[num_parts++] = part;
        }
        part = strtok(NULL, "/");
    }

    if (num_parts == 1 && strcmp(parts[0], "/") == 0) {
        strcpy(norm_path, "/");
    } else {
        norm_path[0] = '\0';
        for (int i = 0; i < num_parts; i++) {
            if (i == 0 && strcmp(parts[0], "/") == 0) {
                strcat(norm_path, "/");
            } else {
                strcat(norm_path, parts[i]);
                if (i < num_parts - 1) strcat(norm_path, "/");
            }
        }
    }
    return norm_path;
}

static int shell_is_dir(const char *path)
{
    FILINFO info;
    char    lfn[SHELL_LFN_SIZE];
    info.lfname = lfn;
    info.lfsize = sizeof(lfn);
    if (f_stat(fatfs_path(path), &info) == FR_OK)
        return (info.fattrib & AM_DIR) != 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 命令实现                                                             */
/* ------------------------------------------------------------------ */
static int cmd_help(int argc, char *argv[])
{
    shell_printf("Available commands:\r\n");
    for (int i = 0; i < (int)NUM_COMMANDS; i++)
        shell_printf("  %-8s - %s\r\n", commands[i].name, commands[i].help);
    return 0;
}

static int cmd_ls(int argc, char *argv[])
{
    int show_tree = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tree") == 0) {
            show_tree = 1;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--; i--;
        }
    }

    const char *dir_path = (argc > 1) ? shell_normalize_path(argv[1])
                                      : shell_state.current_dir;

    DIR dir;
    if (f_opendir(&dir, fatfs_path(dir_path)) != FR_OK) {
        shell_printf("Cannot open directory '%s'\r\n", dir_path);
        return -1;
    }

    FILINFO info;
    char    lfn[SHELL_LFN_SIZE];
    info.lfname = lfn;
    info.lfsize = sizeof(lfn);

    shell_printf("Directory '%s':\r\n", dir_path);

    if (show_tree) {
        int count = 0;
        while (f_readdir(&dir, &info) == FR_OK && info.fname[0]) {
            const char *name = get_fname(&info);
            if (info.fattrib & AM_DIR)
                shell_printf("+-- [DIR]  %s\r\n", name);
            else
                shell_printf("+-- [FILE] %s (%lu bytes)\r\n",
                             name, (unsigned long)info.fsize);
            count++;
        }
        if (count == 0) shell_printf("  (empty)\r\n");
    } else {
        shell_printf("  Type   Size     Name\r\n");
        shell_printf("  ----- -------- --------------------\r\n");
        int count = 0;
        while (f_readdir(&dir, &info) == FR_OK && info.fname[0]) {
            const char *name = get_fname(&info);
            const char *type = (info.fattrib & AM_DIR) ? "[DIR] " : "[FILE]";
            shell_printf("  %-5s %8lu %s\r\n", type,
                         (unsigned long)info.fsize, name);
            count++;
        }
        if (count == 0) shell_printf("  (empty)\r\n");
    }

    return 0;
}

static int cmd_cd(int argc, char *argv[])
{
    const char *new_path = (argc < 2) ? "/" : shell_normalize_path(argv[1]);

    DIR dir;
    if (f_opendir(&dir, fatfs_path(new_path)) != FR_OK) {
        shell_printf("Cannot change to '%s'\r\n", new_path);
        return -1;
    }

    strcpy(shell_state.current_dir, new_path);
    shell_printf("Changed to '%s'\r\nContents:\r\n", new_path);

    FILINFO info;
    char    lfn[SHELL_LFN_SIZE];
    info.lfname = lfn;
    info.lfsize = sizeof(lfn);
    int count = 0;

    while (f_readdir(&dir, &info) == FR_OK && info.fname[0]) {
        const char *name = get_fname(&info);
        count++;
        if (info.fattrib & AM_DIR)
            shell_printf("  [DIR]  %s\r\n", name);
        else
            shell_printf("  [FILE] %s (%lu bytes)\r\n",
                         name, (unsigned long)info.fsize);
    }
    if (count == 0) shell_printf("  (empty directory)\r\n");

    return 0;
}

static int cmd_pwd(int argc, char *argv[])
{
    shell_printf("%s\r\n", shell_state.current_dir);
    return 0;
}

static int cmd_cat(int argc, char *argv[])
{
    if (argc < 2) { shell_printf("Usage: cat <file>\r\n"); return -1; }

    char *file_path = shell_normalize_path(argv[1]);
    FIL   file;

    if (f_open(&file, fatfs_path(file_path), FA_READ) != FR_OK) {
        shell_printf("Cannot open '%s'\r\n", file_path);
        return -1;
    }

    char buf[SHELL_MAX_LINE_LEN + 1];
    UINT br;
    while (f_read(&file, buf, SHELL_MAX_LINE_LEN, &br) == FR_OK && br > 0) {
        buf[br] = '\0';
        shell_printf("%s", buf);
    }
    shell_printf("\r\n");
    f_close(&file);
    return 0;
}

static int cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) { shell_printf("Usage: mkdir <dir>\r\n"); return -1; }

    char *dir_path = shell_normalize_path(argv[1]);
    if (f_mkdir(fatfs_path(dir_path)) != FR_OK) {
        shell_printf("Cannot create '%s'\r\n", dir_path);
        return -1;
    }
    shell_printf("Directory '%s' created\r\n", dir_path);
    return 0;
}

static int cmd_rm(int argc, char *argv[])
{
    if (argc < 2) { shell_printf("Usage: rm <path>\r\n"); return -1; }

    char    *path = shell_normalize_path(argv[1]);
    FRESULT  res  = f_unlink(fatfs_path(path));
    if (res != FR_OK) {
        shell_printf("Cannot remove '%s': Error %d\r\n", path, (int)res);
        if (res == FR_DENIED)
            shell_printf("Note: Directory may not be empty\r\n");
        return -1;
    }
    shell_printf("'%s' removed\r\n", path);
    return 0;
}

static int cmd_touch(int argc, char *argv[])
{
    if (argc < 2) { shell_printf("Usage: touch <file>\r\n"); return -1; }

    char *file_path = shell_normalize_path(argv[1]);
    FIL   file;
    if (f_open(&file, fatfs_path(file_path), FA_OPEN_ALWAYS | FA_WRITE) != FR_OK) {
        shell_printf("Cannot create '%s'\r\n", file_path);
        return -1;
    }
    f_close(&file);
    shell_printf("'%s' created/updated\r\n", file_path);
    return 0;
}

static int cmd_mv(int argc, char *argv[])
{
    if (argc < 3) { shell_printf("Usage: mv <src> <dst>\r\n"); return -1; }

    char src[SHELL_MAX_PATH_LEN + 3];
    char dst[SHELL_MAX_PATH_LEN + 3];
    snprintf(src, sizeof(src), "0:%s", shell_normalize_path(argv[1]));
    snprintf(dst, sizeof(dst), "0:%s", shell_normalize_path(argv[2]));

    if (f_rename(src, dst) != FR_OK) {
        shell_printf("Cannot move '%s' to '%s'\r\n", argv[1], argv[2]);
        return -1;
    }
    shell_printf("'%s' moved to '%s'\r\n", argv[1], argv[2]);
    return 0;
}

static int cmd_cp(int argc, char *argv[])
{
    if (argc < 3) { shell_printf("Usage: cp <src> <dst>\r\n"); return -1; }

    char src[SHELL_MAX_PATH_LEN + 3];
    char dst[SHELL_MAX_PATH_LEN + 3];
    snprintf(src, sizeof(src), "0:%s", shell_normalize_path(argv[1]));
    snprintf(dst, sizeof(dst), "0:%s", shell_normalize_path(argv[2]));

    FIL src_file, dst_file;
    if (f_open(&src_file, src, FA_READ) != FR_OK) {
        shell_printf("Cannot open source '%s'\r\n", argv[1]);
        return -1;
    }
    if (f_open(&dst_file, dst, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        shell_printf("Cannot create destination '%s'\r\n", argv[2]);
        f_close(&src_file);
        return -1;
    }

    char buf[128];
    UINT br, bw;
    long total = 0;
    while (f_read(&src_file, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        if (f_write(&dst_file, buf, br, &bw) != FR_OK || bw != br) {
            shell_printf("Write error\r\n");
            f_close(&src_file);
            f_close(&dst_file);
            return -1;
        }
        total += (long)bw;
    }
    f_close(&src_file);
    f_close(&dst_file);
    shell_printf("Copied '%s' to '%s', %ld bytes\r\n", argv[1], argv[2], total);
    return 0;
}

static int cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
        shell_printf("%s%s", argv[i], (i < argc - 1) ? " " : "");
    shell_printf("\r\n");
    return 0;
}

static int cmd_clear(int argc, char *argv[])
{
    shell_printf("\033[2J\033[H");
    return 0;
}

static int cmd_write(int argc, char *argv[])
{
    if (argc < 3) {
        shell_printf("Usage: write <file> <text>\r\n");
        return -1;
    }

    char *file_path = shell_normalize_path(argv[1]);
    FIL   file;
    if (f_open(&file, fatfs_path(file_path), FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        shell_printf("Cannot open '%s' for writing\r\n", file_path);
        return -1;
    }

    UINT bw;
    for (int i = 2; i < argc; i++) {
        f_write(&file, argv[i], strlen(argv[i]), &bw);
        if (i < argc - 1) f_write(&file, " ", 1, &bw);
    }
    f_close(&file);
    shell_printf("Written to '%s'\r\n", file_path);
    return 0;
}
