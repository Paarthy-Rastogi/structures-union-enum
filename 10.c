// Typed Container - Structure + Union + Enumeration
//
// Build: gcc -std=c11 -Wall -Wextra 10.c -o 10
// (Optionally add -fsanitize=address,undefined to check for leaks / invalid accesses.)
//
// ============================== Container API ==============================
// A Container holds exactly one active value: nothing (TYPE_EMPTY), an int, a
// float, a char, or a string. A string is OWNED by the container: it lives in its
// own heap buffer which the container frees when the value is replaced or cleared.
//
// Rules of use
//  * Every Container must be initialised with container_init() before any other call.
//  * Every Container must be released with container_clear() when no longer needed.
//  * Never shallow-copy a Container (c2 = c1) if it may hold a string; use
//    container_copy() instead, which makes an independent deep copy.
//
// Status codes (ContainerStatus)
//  CONTAINER_OK         success
//  CONTAINER_ERR_NULL   a required pointer argument was NULL
//  CONTAINER_ERR_TYPE   getter used on a container of a different type
//                       (or the container holds an invalid tag)
//  CONTAINER_ERR_ALLOC  memory allocation failed; the container keeps its
//                       previous valid value
//
// Functions
//  container_init(c)                     -> make c a valid TYPE_EMPTY container
//  container_clear(c)                    -> free owned memory, become TYPE_EMPTY
//                                           (safe to call repeatedly)
//  container_set_int/float/char(c, v)    -> replace the value (frees an owned string)
//  container_set_string(c, s)            -> store an independent copy of s; s may even
//                                           point inside c's own current string
//  container_get_int/float/char(c, &out) -> status + value via out pointer; `out` is
//                                           left untouched on failure
//  container_get_string(c, &out)         -> out receives a BORROWED pointer, valid only
//                                           until c is modified, copied over or cleared
//  container_copy(dst, src)              -> deep copy; dst must be initialised;
//                                           copying onto itself is a successful no-op;
//                                           on failure dst keeps its previous value
//  container_print(c)                    -> print the value according to its type
//  container_type_name(t)                -> readable label for a DataType
// ===========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TYPE_EMPTY,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_CHAR,
    TYPE_STRING
} DataType;

typedef union {
    int   int_value;
    float float_value;
    char  char_value;
    char *string_value;     /* owned heap buffer when type == TYPE_STRING */
} Data;

typedef struct {
    Data data;
    DataType type;
} Container;

typedef enum {
    CONTAINER_OK = 0,
    CONTAINER_ERR_NULL,
    CONTAINER_ERR_TYPE,
    CONTAINER_ERR_ALLOC
} ContainerStatus;

/* ---------- test hook for the allocation-failure path ---------- */

/* -1 = never fail. n >= 0: the (n+1)-th string allocation from now fails once. */
static long g_alloc_fail_countdown = -1;

void container_test_fail_alloc_after(long n) { g_alloc_fail_countdown = n; }

/* Allocates an independent copy of s (ISO C replacement for strdup). */
static char *alloc_string_copy(const char *s) {
    size_t len = strlen(s);
    char *p;
    if (g_alloc_fail_countdown == 0) {
        g_alloc_fail_countdown = -1;
        return NULL;                       /* simulated failure */
    }
    if (g_alloc_fail_countdown > 0) g_alloc_fail_countdown--;
    p = (char *)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);                 /* includes the '\0' */
    return p;
}

/* ---------- lifecycle ---------- */

ContainerStatus container_init(Container *c) {
    if (!c) return CONTAINER_ERR_NULL;
    memset(&c->data, 0, sizeof c->data);
    c->type = TYPE_EMPTY;
    return CONTAINER_OK;
}

ContainerStatus container_clear(Container *c) {
    if (!c) return CONTAINER_ERR_NULL;
    if (c->type == TYPE_STRING) free(c->data.string_value);
    memset(&c->data, 0, sizeof c->data);
    c->type = TYPE_EMPTY;
    return CONTAINER_OK;
}

/* ---------- setters ---------- */

ContainerStatus container_set_int(Container *c, int value) {
    if (!c) return CONTAINER_ERR_NULL;
    container_clear(c);
    c->data.int_value = value;
    c->type = TYPE_INT;
    return CONTAINER_OK;
}

ContainerStatus container_set_float(Container *c, float value) {
    if (!c) return CONTAINER_ERR_NULL;
    container_clear(c);
    c->data.float_value = value;
    c->type = TYPE_FLOAT;
    return CONTAINER_OK;
}

ContainerStatus container_set_char(Container *c, char value) {
    if (!c) return CONTAINER_ERR_NULL;
    container_clear(c);
    c->data.char_value = value;
    c->type = TYPE_CHAR;
    return CONTAINER_OK;
}

/* The new copy is made BEFORE the old value is released. This (a) preserves the old
   value if allocation fails and (b) stays correct when `value` points into the
   container's own current string. */
ContainerStatus container_set_string(Container *c, const char *value) {
    char *copy;
    if (!c || !value) return CONTAINER_ERR_NULL;
    copy = alloc_string_copy(value);
    if (!copy) return CONTAINER_ERR_ALLOC;
    container_clear(c);
    c->data.string_value = copy;
    c->type = TYPE_STRING;
    return CONTAINER_OK;
}

/* ---------- getters ---------- */

ContainerStatus container_get_int(const Container *c, int *out_value) {
    if (!c || !out_value) return CONTAINER_ERR_NULL;
    if (c->type != TYPE_INT) return CONTAINER_ERR_TYPE;
    *out_value = c->data.int_value;
    return CONTAINER_OK;
}

ContainerStatus container_get_float(const Container *c, float *out_value) {
    if (!c || !out_value) return CONTAINER_ERR_NULL;
    if (c->type != TYPE_FLOAT) return CONTAINER_ERR_TYPE;
    *out_value = c->data.float_value;
    return CONTAINER_OK;
}

ContainerStatus container_get_char(const Container *c, char *out_value) {
    if (!c || !out_value) return CONTAINER_ERR_NULL;
    if (c->type != TYPE_CHAR) return CONTAINER_ERR_TYPE;
    *out_value = c->data.char_value;
    return CONTAINER_OK;
}

ContainerStatus container_get_string(const Container *c, const char **out_value) {
    if (!c || !out_value) return CONTAINER_ERR_NULL;
    if (c->type != TYPE_STRING || !c->data.string_value) return CONTAINER_ERR_TYPE;
    *out_value = c->data.string_value;
    return CONTAINER_OK;
}

/* ---------- deep copy ---------- */

ContainerStatus container_copy(Container *destination, const Container *source) {
    if (!destination || !source) return CONTAINER_ERR_NULL;
    if (destination == source) return CONTAINER_OK;          /* self-copy: nothing to do */

    switch (source->type) {
        case TYPE_EMPTY:
            return container_clear(destination);
        case TYPE_INT:
            return container_set_int(destination, source->data.int_value);
        case TYPE_FLOAT:
            return container_set_float(destination, source->data.float_value);
        case TYPE_CHAR:
            return container_set_char(destination, source->data.char_value);
        case TYPE_STRING:
            if (!source->data.string_value) return CONTAINER_ERR_TYPE;
            /* separate buffer; destination keeps its old value if this fails */
            return container_set_string(destination, source->data.string_value);
        default:
            return CONTAINER_ERR_TYPE;                       /* corrupt tag */
    }
}

/* ---------- display ---------- */

const char *container_type_name(DataType type) {
    switch (type) {
        case TYPE_EMPTY:  return "EMPTY";
        case TYPE_INT:    return "INT";
        case TYPE_FLOAT:  return "FLOAT";
        case TYPE_CHAR:   return "CHAR";
        case TYPE_STRING: return "STRING";
    }
    return "INVALID";
}

const char *container_status_message(ContainerStatus s) {
    switch (s) {
        case CONTAINER_OK:        return "ok";
        case CONTAINER_ERR_NULL:  return "NULL argument";
        case CONTAINER_ERR_TYPE:  return "wrong type";
        case CONTAINER_ERR_ALLOC: return "allocation failed";
    }
    return "unknown status";
}

void container_print(const Container *c) {
    if (!c) { printf("(null container)"); return; }
    switch (c->type) {
        case TYPE_EMPTY:  printf("(empty)"); break;
        case TYPE_INT:    printf("%d", c->data.int_value); break;
        case TYPE_FLOAT:  printf("%.2f", c->data.float_value); break;
        case TYPE_CHAR:
            if (isprint((unsigned char)c->data.char_value))
                printf("'%c'", c->data.char_value);
            else
                printf("(char code %d)", (int)(unsigned char)c->data.char_value);
            break;
        case TYPE_STRING:
            printf("\"%s\"", c->data.string_value ? c->data.string_value : "");
            break;
        default:          printf("(invalid type tag)"); break;
    }
}

/* ============================== demonstration ============================== */

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_failures++; } \
} while (0)

static void print_labeled(const char *label, const Container *c) {
    printf("  %s: %-6s ", label, container_type_name(c ? c->type : (DataType)-1));
    container_print(c);
    printf("\n");
}

static void demo_original(void) {
    Container c;
    int i; float f; char ch; const char *s;

    printf("=== 1. Sequential values in one container (original demo) ===\n");
    container_init(&c);

    container_set_int(&c, 10);
    if (container_get_int(&c, &i) == CONTAINER_OK) printf("  Int   : %d\n", i);
    container_set_float(&c, 3.14f);
    if (container_get_float(&c, &f) == CONTAINER_OK) printf("  Float : %f\n", f);
    container_set_char(&c, 'A');
    if (container_get_char(&c, &ch) == CONTAINER_OK) printf("  Char  : %c\n", ch);
    container_set_string(&c, "Hello, World!");
    if (container_get_string(&c, &s) == CONTAINER_OK) printf("  String: %s\n", s);

    container_clear(&c);
    printf("\n");
}

static void demo_array_and_copy(void) {
    enum { N = 6 };
    Container *arr = (Container *)malloc(N * sizeof *arr);
    Container copy;
    const char *s;

    printf("=== 2. Array of %d mixed containers ===\n", (int)N);
    if (!arr) { printf("  allocation failed\n"); g_failures++; return; }
    for (int i = 0; i < N; i++) container_init(&arr[i]);

    container_set_int(&arr[0], 10);
    container_set_float(&arr[1], 3.14f);
    container_set_char(&arr[2], 'A');
    container_set_string(&arr[3], "Hello");
    container_set_int(&arr[4], 0);
    container_set_string(&arr[5], "Data");

    for (int i = 0; i < N; i++) {
        printf("  [%d] %-6s ", i, container_type_name(arr[i].type));
        container_print(&arr[i]);
        printf("\n");
    }

    printf("\n=== 3. Deep copy of a string container ===\n");
    container_init(&copy);
    CHECK(container_copy(&copy, &arr[5]) == CONTAINER_OK, "copy of arr[5] succeeded");
    container_get_string(&copy, &s);
    CHECK(strcmp(s, "Data") == 0, "copy holds \"Data\"");

    container_set_string(&arr[5], "Changed original");
    print_labeled("original now", &arr[5]);
    print_labeled("copy         ", &copy);
    container_get_string(&copy, &s);
    CHECK(strcmp(s, "Data") == 0, "copy still has the initial text");

    container_clear(&arr[5]);
    container_get_string(&copy, &s);
    CHECK(strcmp(s, "Data") == 0, "clearing the source does not affect the copy");

    container_clear(&copy);
    for (int i = 0; i < N; i++) container_clear(&arr[i]);
    free(arr);
    printf("\n");
}

static void demo_edge_cases(void) {
    Container c, d;
    int i = -1; float f = -1.0f; const char *s = NULL;
    ContainerStatus st;

    container_init(&c);
    container_init(&d);

    printf("=== 4. Edge cases ===\n");

    printf("Valid integer zero is not an error:\n");
    container_set_int(&c, 0);
    st = container_get_int(&c, &i);
    CHECK(st == CONTAINER_OK && i == 0, "get_int returns OK with value 0");

    printf("Wrong-type getter (status reported separately, output untouched):\n");
    i = 123;
    st = container_get_float(&c, &f);
    CHECK(st == CONTAINER_ERR_TYPE && f == -1.0f, "get_float on INT -> ERR_TYPE, out unchanged");
    st = container_get_string(&c, &s);
    CHECK(st == CONTAINER_ERR_TYPE && s == NULL, "get_string on INT -> ERR_TYPE, out unchanged");
    st = container_get_int(NULL, &i);
    CHECK(st == CONTAINER_ERR_NULL && i == 123, "get_int(NULL container) -> ERR_NULL");
    st = container_get_int(&c, NULL);
    CHECK(st == CONTAINER_ERR_NULL, "get_int(NULL out) -> ERR_NULL");

    printf("Empty container:\n");
    container_clear(&c);
    print_labeled("empty", &c);
    CHECK(container_get_int(&c, &i) == CONTAINER_ERR_TYPE, "getter on EMPTY -> ERR_TYPE");
    print_labeled("null ", NULL);

    printf("Empty string is a valid string:\n");
    st = container_set_string(&c, "");
    CHECK(st == CONTAINER_OK, "set_string(\"\") succeeded");
    st = container_get_string(&c, &s);
    CHECK(st == CONTAINER_OK && s && s[0] == '\0', "get_string returns empty string, OK status");
    print_labeled("empty string", &c);

    printf("Transitions STRING -> INT -> STRING -> FLOAT:\n");
    container_set_string(&c, "first");  print_labeled("1", &c);
    container_set_int(&c, 42);          print_labeled("2", &c);
    container_set_string(&c, "second"); print_labeled("3", &c);
    container_set_float(&c, 2.5f);      print_labeled("4", &c);
    CHECK(c.type == TYPE_FLOAT, "final type is FLOAT (old strings were freed)");

    printf("String replacement with the same source pointer:\n");
    container_set_string(&c, "alias test");
    container_get_string(&c, &s);
    st = container_set_string(&c, s);                 /* source == own buffer */
    container_get_string(&c, &s);
    CHECK(st == CONTAINER_OK && strcmp(s, "alias test") == 0, "set_string(own pointer) keeps value");
    st = container_set_string(&c, s + 6);             /* pointer INTO own buffer */
    container_get_string(&c, &s);
    CHECK(st == CONTAINER_OK && strcmp(s, "test") == 0, "set_string(pointer inside own buffer) works");

    printf("Allocation failure preserves the previous value:\n");
    container_set_string(&c, "keep me");
    container_test_fail_alloc_after(0);
    st = container_set_string(&c, "replacement");
    container_get_string(&c, &s);
    CHECK(st == CONTAINER_ERR_ALLOC && strcmp(s, "keep me") == 0, "failed set_string leaves \"keep me\" intact");
    container_set_int(&c, 77);
    container_test_fail_alloc_after(0);
    st = container_set_string(&c, "replacement");
    CHECK(st == CONTAINER_ERR_ALLOC && c.type == TYPE_INT && c.data.int_value == 77,
          "failed set_string leaves INT 77 intact");

    printf("Copy semantics:\n");
    container_set_string(&c, "copy me");
    container_set_int(&d, 5);
    container_test_fail_alloc_after(0);
    st = container_copy(&d, &c);
    CHECK(st == CONTAINER_ERR_ALLOC && d.type == TYPE_INT && d.data.int_value == 5,
          "copy allocation failure leaves destination unchanged");
    st = container_copy(&d, &c);
    container_get_string(&d, &s);
    CHECK(st == CONTAINER_OK && strcmp(s, "copy me") == 0, "deep copy of string succeeded");
    CHECK(d.data.string_value != c.data.string_value, "copy uses a separate buffer");
    st = container_copy(&c, &c);
    container_get_string(&c, &s);
    CHECK(st == CONTAINER_OK && strcmp(s, "copy me") == 0, "self-copy is a safe no-op");
    CHECK(container_copy(NULL, &c) == CONTAINER_ERR_NULL &&
          container_copy(&d, NULL) == CONTAINER_ERR_NULL, "copy with NULL argument -> ERR_NULL");
    container_clear(&c);
    container_copy(&d, &c);                           /* copy EMPTY over a string */
    CHECK(d.type == TYPE_EMPTY, "copying an EMPTY source empties the destination");

    printf("Repeated clear and invalid tag:\n");
    container_set_string(&c, "to clear");
    container_clear(&c);
    container_clear(&c);
    container_clear(&c);
    CHECK(c.type == TYPE_EMPTY, "clearing 3 times is safe");
    CHECK(container_clear(NULL) == CONTAINER_ERR_NULL, "clear(NULL) -> ERR_NULL");
    c.type = (DataType)99;                            /* simulate corruption */
    print_labeled("corrupt", &c);
    CHECK(container_get_int(&c, &i) == CONTAINER_ERR_TYPE, "getter on invalid tag -> ERR_TYPE");
    container_clear(&c);
    CHECK(c.type == TYPE_EMPTY, "clear recovers a container with an invalid tag");

    container_clear(&c);
    container_clear(&d);
    printf("\n");
}

int main(void) {
    demo_original();
    demo_array_and_copy();
    demo_edge_cases();

    if (g_failures == 0) printf("All checks passed.\n");
    else printf("%d check(s) FAILED.\n", g_failures);
    return g_failures ? 1 : 0;
}