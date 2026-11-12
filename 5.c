// Structures - 2 : Rectangle Inventory and Geometry Analysis
//
// Design notes
//  * Rectangles live in a dynamically allocated array held by RectangleList
//    (data / count / capacity).
//  * Limits: 1..MAX_RECTANGLES (1000) rectangles, IDs are positive ints.
//  * Dimensions must be finite and > 0. NaN, +-inf, zero, negative values and
//    values whose area/perimeter would overflow to infinity are rejected.
//  * Square test uses a RELATIVE tolerance: |w - h| <= SQUARE_TOLERANCE * max(w, h).
//  * Sorting: ascending by area, ties broken by ascending ID.
//  * Capacity: grows by doubling (capped at MAX_RECTANGLES) and shrinks by half
//    when count <= capacity/4. A failed grow is reported and the old data is
//    kept intact. A failed shrink is harmless and ignored.
//  * Build with -DFORCE_RESIZE_FAIL to simulate realloc failure (testing).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#define MAX_RECTANGLES   1000
#define MIN_CAPACITY     4
#define SQUARE_TOLERANCE 1e-4f
#define LINE_SIZE        128

typedef struct {
    int   id;
    float width;
    float height;
    float area;
    float perimeter;
} Rectangle;

typedef struct {
    Rectangle *data;
    int count;
    int capacity;
} RectangleList;

typedef enum {
    RECT_OK = 0,
    RECT_ERR_INVALID,
    RECT_ERR_DUPLICATE,
    RECT_ERR_NOT_FOUND,
    RECT_ERR_FULL,
    RECT_ERR_MEMORY
} RectStatus;

/* ---------- geometry helpers ---------- */

void calculate_area(Rectangle *r) {
    r->area = r->width * r->height;
}

void calculate_perimeter(Rectangle *r) {
    r->perimeter = 2.0f * (r->width + r->height);
}

void update_derived_fields(Rectangle *r) {
    calculate_area(r);
    calculate_perimeter(r);
}

/* Finite, positive dimensions and finite derived values. */
int is_valid_dimensions(float width, float height) {
    Rectangle t;
    if (!isfinite(width) || !isfinite(height)) return 0;
    if (width <= 0.0f || height <= 0.0f) return 0;
    t.width = width;
    t.height = height;
    update_derived_fields(&t);
    return isfinite(t.area) && isfinite(t.perimeter);
}

int is_square(const Rectangle *r) {
    float larger = fmaxf(r->width, r->height);
    return fabsf(r->width - r->height) <= SQUARE_TOLERANCE * larger;
}

/* ---------- memory management ---------- */

Rectangle *create_rectangle_array(int size) {
    if (size <= 0) return NULL;
    return (Rectangle *)malloc((size_t)size * sizeof(Rectangle));
}

int init_list(RectangleList *list, int capacity) {
    list->data = create_rectangle_array(capacity);
    list->count = 0;
    list->capacity = list->data ? capacity : 0;
    return list->data != NULL;
}

/* Returns 1 on success, 0 on failure (old array stays valid). */
static int resize_array(RectangleList *list, int new_capacity) {
#ifdef FORCE_RESIZE_FAIL
    (void)list; (void)new_capacity;
    return 0;
#else
    Rectangle *p;
    if (new_capacity < list->count || new_capacity <= 0) return 0;
    p = (Rectangle *)realloc(list->data, (size_t)new_capacity * sizeof(Rectangle));
    if (!p) return 0;
    list->data = p;
    list->capacity = new_capacity;
    return 1;
#endif
}

void free_rectangle_array(RectangleList *list) {
    free(list->data);
    list->data = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ---------- core operations ---------- */

int find_index_by_id(const RectangleList *list, int id) {
    for (int i = 0; i < list->count; i++)
        if (list->data[i].id == id) return i;
    return -1;
}

RectStatus add_rectangle(RectangleList *list, int id, float width, float height) {
    if (id <= 0 || !is_valid_dimensions(width, height)) return RECT_ERR_INVALID;
    if (find_index_by_id(list, id) != -1) return RECT_ERR_DUPLICATE;

    if (list->count == list->capacity) {
        int new_cap;
        if (list->capacity >= MAX_RECTANGLES) return RECT_ERR_FULL;
        new_cap = list->capacity * 2;
        if (new_cap < MIN_CAPACITY) new_cap = MIN_CAPACITY;
        if (new_cap > MAX_RECTANGLES) new_cap = MAX_RECTANGLES;
        if (!resize_array(list, new_cap)) return RECT_ERR_MEMORY;
    }

    Rectangle *r = &list->data[list->count];
    r->id = id;
    r->width = width;
    r->height = height;
    update_derived_fields(r);
    list->count++;
    return RECT_OK;
}

void print_rectangle(const Rectangle *r) {
    printf("ID %d: Width = %.2f, Height = %.2f, Area = %.2f, Perimeter = %.2f, %s\n",
           r->id, r->width, r->height, r->area, r->perimeter,
           is_square(r) ? "Square" : "Non-square");
}

void display_rectangles(const RectangleList *list) {
    if (list->count == 0) {
        printf("No rectangles stored.\n");
        return;
    }
    for (int i = 0; i < list->count; i++)
        print_rectangle(&list->data[i]);
}

RectStatus search_rectangle(const RectangleList *list, int id) {
    int idx = find_index_by_id(list, id);
    if (idx < 0) return RECT_ERR_NOT_FOUND;
    print_rectangle(&list->data[idx]);
    return RECT_OK;
}

RectStatus update_rectangle(RectangleList *list, int id, float width, float height) {
    int idx = find_index_by_id(list, id);
    if (idx < 0) return RECT_ERR_NOT_FOUND;
    if (!is_valid_dimensions(width, height)) return RECT_ERR_INVALID;
    list->data[idx].width = width;
    list->data[idx].height = height;
    update_derived_fields(&list->data[idx]);   /* keep area/perimeter consistent */
    return RECT_OK;
}

RectStatus delete_rectangle(RectangleList *list, int id) {
    int idx = find_index_by_id(list, id);
    if (idx < 0) return RECT_ERR_NOT_FOUND;
    for (int i = idx; i < list->count - 1; i++)
        list->data[i] = list->data[i + 1];
    list->count--;
    if (list->capacity > MIN_CAPACITY && list->count <= list->capacity / 4)
        resize_array(list, list->capacity / 2);   /* failure is harmless */
    return RECT_OK;
}

/* ---------- statistics ---------- */

double total_area(const RectangleList *list) {
    double sum = 0.0;
    for (int i = 0; i < list->count; i++) sum += list->data[i].area;
    return sum;
}

/* Returns 0 if the list is empty. Ties: the first one in storage order wins. */
int find_extreme_areas(const RectangleList *list, const Rectangle **largest,
                       const Rectangle **smallest) {
    if (list->count == 0) return 0;
    *largest = *smallest = &list->data[0];
    for (int i = 1; i < list->count; i++) {
        if (list->data[i].area > (*largest)->area)  *largest  = &list->data[i];
        if (list->data[i].area < (*smallest)->area) *smallest = &list->data[i];
    }
    return 1;
}

void show_statistics(const RectangleList *list) {
    const Rectangle *big, *small;
    if (!find_extreme_areas(list, &big, &small)) {
        printf("No rectangles stored: statistics unavailable.\n");
        return;
    }
    printf("Largest area : "); print_rectangle(big);
    printf("Smallest area: "); print_rectangle(small);
    printf("Total area   : %.2f\n", total_area(list));
    printf("Average area : %.2f\n", total_area(list) / list->count);
}

/* ---------- sorting and filtering ---------- */

static int compare_by_area_then_id(const void *a, const void *b) {
    const Rectangle *x = *(const Rectangle *const *)a;
    const Rectangle *y = *(const Rectangle *const *)b;
    if (x->area < y->area) return -1;
    if (x->area > y->area) return 1;
    return (x->id > y->id) - (x->id < y->id);
}

RectStatus display_sorted_by_area(const RectangleList *list) {
    const Rectangle **ptrs;
    if (list->count == 0) {
        printf("No rectangles stored.\n");
        return RECT_OK;
    }
    ptrs = (const Rectangle **)malloc((size_t)list->count * sizeof(*ptrs));
    if (!ptrs) return RECT_ERR_MEMORY;
    for (int i = 0; i < list->count; i++) ptrs[i] = &list->data[i];
    qsort(ptrs, (size_t)list->count, sizeof(*ptrs), compare_by_area_then_id);
    for (int i = 0; i < list->count; i++) print_rectangle(ptrs[i]);
    free(ptrs);
    return RECT_OK;
}

void filter_by_area(const RectangleList *list, float threshold) {
    int found = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->data[i].area > threshold) {
            print_rectangle(&list->data[i]);
            found++;
        }
    }
    if (!found) printf("No rectangle has area greater than %.2f.\n", threshold);
}

/* ---------- input helpers (1 = ok, 0 = EOF) ---------- */

static int read_line(char *buf, size_t n) {
    size_t len;
    if (!fgets(buf, (int)n, stdin)) return 0;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else if (len == n - 1) {               /* overlong line: discard rest, force invalid */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        buf[0] = '\0';
    }
    return 1;
}

static int only_spaces_left(const char *p) {
    while (*p) { if (!isspace((unsigned char)*p)) return 0; p++; }
    return 1;
}

static int parse_int(const char *s, long *out) {
    char *end;
    errno = 0;
    *out = strtol(s, &end, 10);
    return end != s && errno == 0 && only_spaces_left(end);
}

static int parse_float(const char *s, float *out) {
    char *end;
    errno = 0;
    *out = strtof(s, &end);
    return end != s && only_spaces_left(end);   /* validity checked by caller */
}

/* Prompt until an int in [lo, hi] is entered. Returns 0 on EOF. */
static int prompt_int(const char *prompt, long lo, long hi, int *out) {
    char line[LINE_SIZE];
    long v;
    for (;;) {
        printf("%s", prompt);
        if (!read_line(line, sizeof line)) return 0;
        if (parse_int(line, &v) && v >= lo && v <= hi) { *out = (int)v; return 1; }
        printf("Invalid input. Enter an integer between %ld and %ld.\n", lo, hi);
    }
}

/* Prompt until a finite positive float is entered. Returns 0 on EOF. */
static int prompt_positive_float(const char *prompt, float *out) {
    char line[LINE_SIZE];
    float v;
    for (;;) {
        printf("%s", prompt);
        if (!read_line(line, sizeof line)) return 0;
        if (parse_float(line, &v) && isfinite(v) && v > 0.0f) { *out = v; return 1; }
        printf("Invalid input. Enter a finite number greater than 0.\n");
    }
}

static void report_status(RectStatus s) {
    switch (s) {
        case RECT_OK:            break;
        case RECT_ERR_INVALID:   printf("Error: invalid ID or dimensions.\n"); break;
        case RECT_ERR_DUPLICATE: printf("Error: duplicate ID.\n"); break;
        case RECT_ERR_NOT_FOUND: printf("Error: rectangle ID not found.\n"); break;
        case RECT_ERR_FULL:      printf("Error: maximum of %d rectangles reached.\n", MAX_RECTANGLES); break;
        case RECT_ERR_MEMORY:    printf("Error: memory (re)allocation failed; data unchanged.\n"); break;
    }
}

/* Interactive add: re-prompts until add_rectangle succeeds or a fatal status occurs. */
static int input_and_add_rectangle(RectangleList *list) {
    int id;
    float w, h;
    for (;;) {
        RectStatus s;
        if (!prompt_int("  Rectangle ID (positive integer): ", 1, 2147483647L, &id)) return 0;
        if (find_index_by_id(list, id) != -1) { report_status(RECT_ERR_DUPLICATE); continue; }
        if (!prompt_positive_float("  Width : ", &w)) return 0;
        if (!prompt_positive_float("  Height: ", &h)) return 0;
        s = add_rectangle(list, id, w, h);
        if (s == RECT_OK) return 1;
        report_status(s);
        if (s == RECT_ERR_FULL || s == RECT_ERR_MEMORY) return 1;   /* cannot succeed by retrying */
    }
}

static void menu(void) {
    printf("\n===== Rectangle Inventory =====\n"
           "1. Add rectangle\n2. Display all\n3. Search by ID\n4. Update by ID\n"
           "5. Delete by ID\n6. Statistics (largest/smallest/total/average)\n"
           "7. List sorted by area\n8. Filter by area threshold\n0. Exit\n");
}

int main(void) {
    RectangleList list;
    int n, choice, id;
    float w, h, threshold;

    if (!prompt_int("Number of rectangles (1-1000): ", 1, MAX_RECTANGLES, &n)) return 0;
    if (!init_list(&list, n)) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        printf("Rectangle %d of %d:\n", i + 1, n);
        if (!input_and_add_rectangle(&list)) { free_rectangle_array(&list); return 0; }
        if (list.count <= i) break;           /* add failed fatally (e.g. memory) */
    }

    for (;;) {
        menu();
        if (!prompt_int("Choice: ", 0, 8, &choice)) break;
        if (choice == 0) break;

        switch (choice) {
            case 1:
                if (!input_and_add_rectangle(&list)) goto done;
                break;
            case 2:
                display_rectangles(&list);
                break;
            case 3:
                if (!prompt_int("ID to search: ", 1, 2147483647L, &id)) goto done;
                report_status(search_rectangle(&list, id));
                break;
            case 4:
                if (!prompt_int("ID to update: ", 1, 2147483647L, &id)) goto done;
                if (find_index_by_id(&list, id) < 0) { report_status(RECT_ERR_NOT_FOUND); break; }
                if (!prompt_positive_float("  New width : ", &w)) goto done;
                if (!prompt_positive_float("  New height: ", &h)) goto done;
                {
                    RectStatus s = update_rectangle(&list, id, w, h);
                    if (s == RECT_OK) printf("Updated.\n"); else report_status(s);
                }
                break;
            case 5:
                if (!prompt_int("ID to delete: ", 1, 2147483647L, &id)) goto done;
                {
                    RectStatus s = delete_rectangle(&list, id);
                    if (s == RECT_OK) printf("Deleted.\n"); else report_status(s);
                }
                break;
            case 6:
                show_statistics(&list);
                break;
            case 7:
                report_status(display_sorted_by_area(&list));
                break;
            case 8:
                if (!prompt_positive_float("Area threshold: ", &threshold)) goto done;
                filter_by_area(&list, threshold);
                break;
        }
    }

done:
    free_rectangle_array(&list);
    return 0;
}