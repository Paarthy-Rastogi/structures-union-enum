// Unions - Memory Representation and Safe Tagged Access
//
// Build: gcc -std=c11 -Wall -Wextra 6.c -o 6 -lm
//
// Rules this program follows:
//  * A union member is only ever read when it is the member that was last written
//    (tracked by an enum tag). Inactive members are never read or printed.
//  * Only addresses, sizes, alignments and offsets are printed for layout analysis;
//    these never read the union's contents.
//  * No specific memory layout is assumed: everything is measured with
//    sizeof / alignof / offsetof on the compilation platform.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>     /* offsetof */
#include <stdalign.h>   /* alignof  */
#include <math.h>       /* isfinite */

#define TEXT_SIZE 20    /* includes the '\0' terminator -> max 19 characters */
#define RECORD_COUNT 100

typedef union {
    int   i;
    float f;
    char  str[TEXT_SIZE];
} UnionData;

typedef struct {
    int   i;
    float f;
    char  str[TEXT_SIZE];
} StructData;

typedef enum { VALUE_INT, VALUE_FLOAT, VALUE_TEXT } ValueType;

typedef struct {
    ValueType type;     /* which member of 'value' is currently valid */
    UnionData value;
} TaggedValue;

typedef enum {
    SET_OK = 0,
    SET_ERR_NULL,       /* NULL record or NULL string */
    SET_ERR_TOO_LONG,   /* string does not fit with its terminator */
    SET_ERR_BAD_FLOAT   /* NaN or infinity */
} SetStatus;

/* ---------- tagged record API ---------- */

SetStatus set_int(TaggedValue *v, int x) {
    if (!v) return SET_ERR_NULL;
    v->value.i = x;
    v->type = VALUE_INT;
    return SET_OK;
}

SetStatus set_float(TaggedValue *v, float x) {
    if (!v) return SET_ERR_NULL;
    if (!isfinite(x)) return SET_ERR_BAD_FLOAT;
    v->value.f = x;
    v->type = VALUE_FLOAT;
    return SET_OK;
}

/* On failure the record is left completely unchanged. */
SetStatus set_text(TaggedValue *v, const char *s) {
    size_t len;
    if (!v || !s) return SET_ERR_NULL;
    len = strlen(s);
    if (len >= TEXT_SIZE) return SET_ERR_TOO_LONG;   /* need room for '\0' */
    memset(v->value.str, 0, TEXT_SIZE);
    memcpy(v->value.str, s, len);                    /* terminated by the memset */
    v->type = VALUE_TEXT;
    return SET_OK;
}

const char *status_message(SetStatus s) {
    switch (s) {
        case SET_OK:            return "ok";
        case SET_ERR_NULL:      return "rejected: NULL argument";
        case SET_ERR_TOO_LONG:  return "rejected: string too long (max 19 chars + '\\0')";
        case SET_ERR_BAD_FLOAT: return "rejected: float is NaN or infinite";
    }
    return "unknown status";
}

/* Prints ONLY the active member, chosen by the tag. */
void display_value(const TaggedValue *v) {
    if (!v) { printf("(null record)\n"); return; }
    switch (v->type) {
        case VALUE_INT:   printf("INT   : %d\n", v->value.i);    break;
        case VALUE_FLOAT: printf("FLOAT : %.2f\n", v->value.f);  break;
        case VALUE_TEXT:
            /* str was fully zero-filled before the copy, so it is terminated,
               but we still bound the read to be extra safe. */
            printf("TEXT  : \"%.*s\"\n", TEXT_SIZE - 1, v->value.str);
            break;
        default:
            printf("INVALID TAG (%d): value not displayed\n", (int)v->type);
            break;
    }
}

/* ---------- layout analysis ---------- */

static void print_type_info(const char *name, size_t size, size_t align) {
    printf("%-12s size = %3zu bytes, alignment = %zu\n", name, size, align);
}

void show_sizes_and_alignment(void) {
    printf("=== 1. Size and alignment on this platform ===\n");
    print_type_info("UnionData",  sizeof(UnionData),  alignof(UnionData));
    print_type_info("StructData", sizeof(StructData), alignof(StructData));
    print_type_info("TaggedValue", sizeof(TaggedValue), alignof(TaggedValue));
    print_type_info("int",   sizeof(int),   alignof(int));
    print_type_info("float", sizeof(float), alignof(float));
    print_type_info("char[20]", sizeof(char[TEXT_SIZE]), alignof(char[TEXT_SIZE]));
    printf("\n");
}

void show_member_addresses(void) {
    UnionData  u;
    StructData s;

    printf("=== 2. Member addresses (addresses only, no contents read) ===\n");
    printf("Union:\n");
    printf("  &u       : %p\n", (void *)&u);
    printf("  &u.i     : %p\n", (void *)&u.i);
    printf("  &u.f     : %p\n", (void *)&u.f);
    printf("  &u.str   : %p\n", (void *)u.str);
    printf("  All union members start at the same address: %s\n",
           ((void *)&u.i == (void *)&u.f && (void *)&u.f == (void *)u.str) ? "YES" : "NO");

    printf("Structure:\n");
    printf("  &s       : %p\n", (void *)&s);
    printf("  &s.i     : %p\n", (void *)&s.i);
    printf("  &s.f     : %p\n", (void *)&s.f);
    printf("  &s.str   : %p\n", (void *)s.str);
    printf("  All structure members have distinct addresses: %s\n",
           ((void *)&s.i != (void *)&s.f && (void *)&s.f != (void *)s.str &&
            (void *)&s.i != (void *)s.str) ? "YES" : "NO");
    printf("\n");
}

void show_struct_offsets(void) {
    size_t sum = sizeof(int) + sizeof(float) + TEXT_SIZE;
    size_t tail_end = offsetof(StructData, str) + TEXT_SIZE;

    printf("=== 3. offsetof for StructData ===\n");
    printf("  offsetof(i)   = %zu (size %zu, alignment %zu)\n",
           offsetof(StructData, i),   sizeof(int),   alignof(int));
    printf("  offsetof(f)   = %zu (size %zu, alignment %zu)\n",
           offsetof(StructData, f),   sizeof(float), alignof(float));
    printf("  offsetof(str) = %zu (size %zu, alignment %zu)\n",
           offsetof(StructData, str), (size_t)TEXT_SIZE, alignof(char));
    printf("  Sum of member sizes     = %zu\n", sum);
    printf("  Members end at offset   = %zu\n", tail_end);
    printf("  sizeof(StructData)      = %zu\n", sizeof(StructData));
    printf("  Total padding           = %zu byte(s) (inside: %zu, trailing: %zu)\n",
           sizeof(StructData) - sum,
           tail_end - sum,
           sizeof(StructData) - tail_end);
    printf("  Each offset is a multiple of its member's alignment; the compiler\n"
           "  inserts padding where needed, and may add trailing padding so that\n"
           "  sizeof is a multiple of alignof(StructData) (array elements stay aligned).\n");
    printf("  offsetof for union members: i=%zu f=%zu str=%zu (always 0 for a union)\n",
           offsetof(UnionData, i), offsetof(UnionData, f), offsetof(UnionData, str));
    printf("\n");
}

void compare_array_memory(void) {
    printf("=== 4. Memory for %d records ===\n", RECORD_COUNT);
    printf("  %d x StructData  = %zu bytes\n", RECORD_COUNT, (size_t)RECORD_COUNT * sizeof(StructData));
    printf("  %d x UnionData   = %zu bytes\n", RECORD_COUNT, (size_t)RECORD_COUNT * sizeof(UnionData));
    printf("  %d x TaggedValue = %zu bytes\n", RECORD_COUNT, (size_t)RECORD_COUNT * sizeof(TaggedValue));
    printf("  Union saves %zu bytes vs. struct (tagged record saves %lld).\n",
           (size_t)RECORD_COUNT * (sizeof(StructData) - sizeof(UnionData)),
           (long long)RECORD_COUNT * ((long long)sizeof(StructData) - (long long)sizeof(TaggedValue)));
    printf("\n");
}

/* ---------- tagged record demonstrations ---------- */

static void try_set_text(TaggedValue *v, const char *s) {
    SetStatus st = set_text(v, s);
    printf("  set_text(\"%s\") -> %s\n", s, status_message(st));
}

void demo_mixed_records(void) {
    TaggedValue recs[5];
    memset(recs, 0, sizeof recs);

    printf("=== 5. Five tagged records of mixed types ===\n");
    set_int(&recs[0], 10);
    set_float(&recs[1], 220.5f);
    set_text(&recs[2], "Union Programming");
    set_int(&recs[3], -42);
    set_text(&recs[4], "Tagged");

    for (int i = 0; i < 5; i++) {
        printf("  Record %d -> ", i + 1);
        display_value(&recs[i]);
    }
    printf("  sizeof(TaggedValue) = %zu, sizeof(UnionData) = %zu, sizeof(StructData) = %zu\n\n",
           sizeof(TaggedValue), sizeof(UnionData), sizeof(StructData));
}

void demo_edge_cases(void) {
    TaggedValue v;
    SetStatus st;
    memset(&v, 0, sizeof v);

    printf("=== 6. Edge cases ===\n");

    printf("Maximum-length string (19 chars) and overlength strings:\n");
    try_set_text(&v, "1234567890123456789");     /* 19 chars: fits */
    printf("  now holds: "); display_value(&v);
    try_set_text(&v, "12345678901234567890");     /* 20 chars: no room for '\0' */
    printf("  after rejected 20-char string, record still: "); display_value(&v);
    try_set_text(&v, "This string is far too long for the buffer");
    printf("  after rejected long string, record still:     "); display_value(&v);

    printf("Switching one record int -> text -> float:\n");
    set_int(&v, 7);          printf("  as int  : "); display_value(&v);
    set_text(&v, "seven");   printf("  as text : "); display_value(&v);
    set_float(&v, 7.0f);     printf("  as float: "); display_value(&v);

    printf("Rejected inputs (record must stay unchanged):\n");
    st = set_float(&v, NAN);
    printf("  set_float(NAN) -> %s; record: ", status_message(st)); display_value(&v);
    st = set_float(&v, INFINITY);
    printf("  set_float(INF) -> %s; record: ", status_message(st)); display_value(&v);
    st = set_text(&v, NULL);
    printf("  set_text(NULL) -> %s; record: ", status_message(st)); display_value(&v);
    st = set_int(NULL, 1);
    printf("  set_int(NULL record) -> %s\n", status_message(st));

    printf("Invalid tag handling:\n");
    v.type = (ValueType)99;   /* simulate corruption */
    printf("  "); display_value(&v);
    display_value(NULL);
    printf("\n");
}

void print_observations(void) {
    printf("=== 7. Observations ===\n"
"  * Union members share one block: all offsets are 0 and all addresses are equal,\n"
"    so sizeof(UnionData) is the size of its largest member (the 20-byte char array),\n"
"    rounded up to the union's alignment. StructData stores every member separately,\n"
"    so it is at least the sum of its members, plus any alignment padding.\n"
"  * Alignment: each member must sit at an offset that is a multiple of its alignment;\n"
"    the whole type's size is rounded up to a multiple of its strictest member's\n"
"    alignment. Exact numbers are platform/compiler dependent, which is why the\n"
"    program measures them with sizeof, alignof and offsetof.\n"
"  * Saving memory: when a record holds ONE of several types at a time, a union\n"
"    stores only the largest one. For 100 records the saving is shown in section 4.\n"
"  * The price: values do not coexist. Writing one member overwrites the shared bytes,\n"
"    so the others become stale (reading a different member than the one last written\n"
"    reinterprets bytes and can print garbage, or an unterminated 'string').\n"
"    The enum tag records which member is valid, and display_value() reads only that one.\n"
"  * The tag costs space: TaggedValue is larger than UnionData (tag + padding), but it\n"
"    is typically still smaller than StructData, and it makes access safe.\n");
}

int main(void) {
    show_sizes_and_alignment();
    show_member_addresses();
    show_struct_offsets();
    compare_array_memory();
    demo_mixed_records();
    demo_edge_cases();
    print_observations();
    return 0;
}