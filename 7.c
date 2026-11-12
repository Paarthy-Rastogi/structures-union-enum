// Enumerations - Interactive Command-Driven Task Manager
//
// Build: gcc -std=c11 -Wall -Wextra 7.c -o 7
//        (add -DFORCE_REALLOC_FAIL to simulate a realloc failure when testing)
//
// Design notes
//  * Commands are case-insensitive (the input is upper-cased before matching).
//  * All input is read with fgets into bounded buffers. A line that does not fit is
//    detected, the rest of it is discarded, and the input is rejected.
//  * Limits: command <= 31 chars, description <= 79 chars, task ID 1..INT_MAX.
//  * The task array starts with a small capacity and doubles with realloc when full.
//    If realloc fails, the existing tasks are kept and the ADD is rejected.
//  * EOF at any prompt ends the session cleanly (memory is freed).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define CMD_BUF_SIZE   32
#define DESC_BUF_SIZE  80
#define ID_BUF_SIZE    32
#define INITIAL_CAPACITY 2

typedef enum {
    CMD_ADD,
    CMD_DELETE,
    CMD_VIEW,
    CMD_UPDATE,
    CMD_COMPLETE,
    CMD_FIND,
    CMD_HELP,
    CMD_EXIT,
    CMD_UNKNOWN
} CommandType;

typedef enum { TASK_PENDING, TASK_DONE } TaskStatus;

typedef struct {
    int id;
    char description[DESC_BUF_SIZE];
    TaskStatus status;
} Task;

typedef struct {
    Task *items;
    int count;
    int capacity;
} TaskList;

typedef enum { LINE_OK, LINE_EOF, LINE_TOO_LONG } LineStatus;
typedef enum { RES_CONTINUE, RES_EOF, RES_EXIT } CmdResult;

/* ---------- command table: used by the parser and by HELP ---------- */

static const struct {
    const char *name;
    CommandType type;
    const char *help;
} COMMAND_TABLE[] = {
    { "ADD",      CMD_ADD,      "add a new task (asks for ID and description)" },
    { "DELETE",   CMD_DELETE,   "delete a task by ID" },
    { "VIEW",     CMD_VIEW,     "list all tasks" },
    { "UPDATE",   CMD_UPDATE,   "change the description of a task" },
    { "COMPLETE", CMD_COMPLETE, "mark a task as DONE" },
    { "FIND",     CMD_FIND,     "show one task by ID" },
    { "HELP",     CMD_HELP,     "show this list of commands" },
    { "EXIT",     CMD_EXIT,     "quit the program" }
};
#define COMMAND_COUNT (sizeof COMMAND_TABLE / sizeof COMMAND_TABLE[0])

/* Case-insensitive mapping of text to enum. */
CommandType get_command_type(const char *text) {
    char upper[CMD_BUF_SIZE];
    size_t i;
    if (!text || strlen(text) >= sizeof upper) return CMD_UNKNOWN;
    for (i = 0; text[i]; i++) upper[i] = (char)toupper((unsigned char)text[i]);
    upper[i] = '\0';
    for (i = 0; i < COMMAND_COUNT; i++)
        if (strcmp(upper, COMMAND_TABLE[i].name) == 0) return COMMAND_TABLE[i].type;
    return CMD_UNKNOWN;
}

/* ---------- input helpers ---------- */

static void trim(char *s) {
    size_t len = strlen(s), start = 0;
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start) memmove(s, s + start, strlen(s + start) + 1);
}

/* Reads one line (without '\n'), trimmed. Overlong lines are drained and reported. */
static LineStatus read_line(char *buf, size_t size) {
    size_t len;
    if (!fgets(buf, (int)size, stdin)) { buf[0] = '\0'; return LINE_EOF; }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else if (len == size - 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        buf[0] = '\0';
        return LINE_TOO_LONG;
    }
    trim(buf);
    return LINE_OK;
}

/* Prompts for an ID. Returns 1 = ok, 0 = invalid (message printed), -1 = EOF. */
static int read_id(const char *prompt, int *out) {
    char buf[ID_BUF_SIZE], *end;
    long v;
    LineStatus st;

    printf("%s", prompt);
    st = read_line(buf, sizeof buf);
    if (st == LINE_EOF) return -1;
    if (st == LINE_TOO_LONG) { printf("Error: ID input too long.\n"); return 0; }
    if (buf[0] == '\0') { printf("Error: missing task ID.\n"); return 0; }
    errno = 0;
    v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0') { printf("Error: '%s' is not a valid number.\n", buf); return 0; }
    if (errno == ERANGE || v > INT_MAX) { printf("Error: ID out of range.\n"); return 0; }
    if (v <= 0) { printf("Error: task ID must be a positive number.\n"); return 0; }
    *out = (int)v;
    return 1;
}

/* Prompts for a non-empty description. Returns 1 = ok, 0 = invalid, -1 = EOF. */
static int read_description(char *buf, size_t size) {
    LineStatus st;
    printf("Description: ");
    st = read_line(buf, size);
    if (st == LINE_EOF) return -1;
    if (st == LINE_TOO_LONG) {
        printf("Error: description too long (max %zu characters).\n", size - 1);
        return 0;
    }
    if (buf[0] == '\0') { printf("Error: description cannot be empty.\n"); return 0; }
    return 1;
}

/* ---------- task list operations ---------- */

int init_list(TaskList *list) {
    list->items = (Task *)malloc(INITIAL_CAPACITY * sizeof(Task));
    list->count = 0;
    list->capacity = list->items ? INITIAL_CAPACITY : 0;
    return list->items != NULL;
}

void free_list(TaskList *list) {
    free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

int find_index(const TaskList *list, int id) {
    for (int i = 0; i < list->count; i++)
        if (list->items[i].id == id) return i;
    return -1;
}

/* Returns 1 on success, 0 if the array could not grow (list unchanged). */
static int grow_if_full(TaskList *list) {
    Task *p;
    int new_cap;
    if (list->count < list->capacity) return 1;
    if (list->capacity > INT_MAX / 2) return 0;
    new_cap = list->capacity ? list->capacity * 2 : INITIAL_CAPACITY;
#ifdef FORCE_REALLOC_FAIL
    p = NULL;
#else
    p = (Task *)realloc(list->items, (size_t)new_cap * sizeof(Task));
#endif
    if (!p) return 0;
    list->items = p;
    list->capacity = new_cap;
    return 1;
}

static const char *status_text(TaskStatus s) {
    return s == TASK_DONE ? "DONE" : "PENDING";
}

static void print_task(const Task *t) {
    printf("  [%d] %-40s %s\n", t->id, t->description, status_text(t->status));
}

/* ---------- one function per command ---------- */

CmdResult cmd_add(TaskList *list) {
    int id, r;
    char desc[DESC_BUF_SIZE];

    if ((r = read_id("Task ID: ", &id)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    if (find_index(list, id) >= 0) { printf("Error: task ID %d already exists.\n", id); return RES_CONTINUE; }
    if ((r = read_description(desc, sizeof desc)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;

    if (!grow_if_full(list)) {
        printf("Error: could not expand task list (out of memory); task %d not added.\n", id);
        return RES_CONTINUE;
    }
    list->items[list->count].id = id;
    strcpy(list->items[list->count].description, desc);   /* length already validated */
    list->items[list->count].status = TASK_PENDING;
    list->count++;
    printf("Task %d added.\n", id);
    return RES_CONTINUE;
}

CmdResult cmd_delete(TaskList *list) {
    int id, r, idx;
    if (list->count == 0) { printf("The task list is empty.\n"); return RES_CONTINUE; }
    if ((r = read_id("Task ID to delete: ", &id)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    idx = find_index(list, id);
    if (idx < 0) { printf("Error: task ID %d not found.\n", id); return RES_CONTINUE; }
    for (int i = idx; i < list->count - 1; i++) list->items[i] = list->items[i + 1];
    list->count--;
    printf("Task %d deleted.\n", id);
    return RES_CONTINUE;
}

CmdResult cmd_view(const TaskList *list) {
    if (list->count == 0) { printf("The task list is empty.\n"); return RES_CONTINUE; }
    printf("Tasks (%d):\n", list->count);
    for (int i = 0; i < list->count; i++) print_task(&list->items[i]);
    return RES_CONTINUE;
}

CmdResult cmd_update(TaskList *list) {
    int id, r, idx;
    char desc[DESC_BUF_SIZE];
    if (list->count == 0) { printf("The task list is empty.\n"); return RES_CONTINUE; }
    if ((r = read_id("Task ID to update: ", &id)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    idx = find_index(list, id);
    if (idx < 0) { printf("Error: task ID %d not found.\n", id); return RES_CONTINUE; }
    if ((r = read_description(desc, sizeof desc)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    strcpy(list->items[idx].description, desc);
    printf("Task %d updated.\n", id);
    return RES_CONTINUE;
}

CmdResult cmd_complete(TaskList *list) {
    int id, r, idx;
    if (list->count == 0) { printf("The task list is empty.\n"); return RES_CONTINUE; }
    if ((r = read_id("Task ID to complete: ", &id)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    idx = find_index(list, id);
    if (idx < 0) { printf("Error: task ID %d not found.\n", id); return RES_CONTINUE; }
    if (list->items[idx].status == TASK_DONE) {
        printf("Task %d is already DONE.\n", id);
        return RES_CONTINUE;
    }
    list->items[idx].status = TASK_DONE;
    printf("Task %d marked DONE.\n", id);
    return RES_CONTINUE;
}

CmdResult cmd_find(const TaskList *list) {
    int id, r, idx;
    if (list->count == 0) { printf("The task list is empty.\n"); return RES_CONTINUE; }
    if ((r = read_id("Task ID to find: ", &id)) <= 0) return r < 0 ? RES_EOF : RES_CONTINUE;
    idx = find_index(list, id);
    if (idx < 0) { printf("Error: task ID %d not found.\n", id); return RES_CONTINUE; }
    print_task(&list->items[idx]);
    return RES_CONTINUE;
}

CmdResult cmd_help(void) {
    printf("Commands (case-insensitive):\n");
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        printf("  %-9s %s\n", COMMAND_TABLE[i].name, COMMAND_TABLE[i].help);
    return RES_CONTINUE;
}

/* ---------- main loop ---------- */

int main(void) {
    TaskList list;
    char input[CMD_BUF_SIZE];
    CmdResult res = RES_CONTINUE;

    if (!init_list(&list)) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    printf("Task Manager. Type HELP for commands.\n");
    while (res == RES_CONTINUE) {
        LineStatus st;
        printf("\n> ");
        st = read_line(input, sizeof input);
        if (st == LINE_EOF) { printf("\nEnd of input. Exiting.\n"); break; }
        if (st == LINE_TOO_LONG) {
            printf("Error: command too long (max %d characters).\n", CMD_BUF_SIZE - 1);
            continue;
        }
        if (input[0] == '\0') continue;           /* blank line: just prompt again */

        switch (get_command_type(input)) {
            case CMD_ADD:      res = cmd_add(&list);      break;
            case CMD_DELETE:   res = cmd_delete(&list);   break;
            case CMD_VIEW:     res = cmd_view(&list);     break;
            case CMD_UPDATE:   res = cmd_update(&list);   break;
            case CMD_COMPLETE: res = cmd_complete(&list); break;
            case CMD_FIND:     res = cmd_find(&list);     break;
            case CMD_HELP:     res = cmd_help();          break;
            case CMD_EXIT:     printf("Exiting program.\n"); res = RES_EXIT; break;
            case CMD_UNKNOWN:
            default:
                printf("Unknown command '%s'. Type HELP for the list of commands.\n", input);
                break;
        }
    }
    if (res == RES_EOF) printf("\nEnd of input. Exiting.\n");

    free_list(&list);
    return 0;
}