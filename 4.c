// Structures - 1 (Extended): Student Records and Academic Analytics System
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

#define NAME_SIZE 50          /* 49 characters + '\0' */
#define LINE_SIZE 256
#define MAX_CAPACITY 1000000  /* upper limit to reject excessively large requests */

typedef struct {
    int roll;
    char name[NAME_SIZE];
    int age;
    float gpa;
} Student;

typedef struct {
    Student *data;
    int count;
    int capacity;
} StudentList;

/* ---------- Input helpers ---------- */

/* Returns 1 = ok, 0 = EOF/no input, -1 = line too long */
static int read_line(char *buf, size_t size) {
    if (fgets(buf, (int)size, stdin) == NULL) return 0;
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        return 1;
    }
    int c, extra = 0;
    while ((c = getchar()) != '\n' && c != EOF) extra = 1;
    return extra ? -1 : 1;
}

static int read_long(const char *prompt, long *out) {
    char buf[LINE_SIZE];
    printf("%s", prompt);
    if (read_line(buf, sizeof buf) <= 0) return 0;
    char *end;
    errno = 0;
    long v = strtol(buf, &end, 10);
    if (end == buf || errno == ERANGE) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

static int read_double(const char *prompt, double *out) {
    char buf[LINE_SIZE];
    printf("%s", prompt);
    if (read_line(buf, sizeof buf) <= 0) return 0;
    char *end;
    errno = 0;
    double v = strtod(buf, &end);
    if (end == buf || errno == ERANGE) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* ---------- Validation ---------- */

static int is_valid_roll(long r) { return r > 0 && r <= INT_MAX; }
static int is_valid_age(long a)  { return a >= 16 && a <= 100; }
static int is_valid_gpa(double g){ return g >= 0.0 && g <= 4.0; }

static int input_roll(int *roll) {
    long r;
    if (!read_long("Enter roll number (positive integer): ", &r) || !is_valid_roll(r)) {
        printf("Error: invalid roll number.\n");
        return 0;
    }
    *roll = (int)r;
    return 1;
}

static int input_name(char *name) {
    char buf[LINE_SIZE];
    printf("Enter name (max 49 characters, spaces allowed): ");
    int r = read_line(buf, sizeof buf);
    if (r < 0 || (r == 1 && strlen(buf) >= NAME_SIZE)) {
        printf("Error: name exceeds 49 characters.\n");
        return 0;
    }
    if (r == 0 || buf[0] == '\0') {
        printf("Error: name cannot be empty.\n");
        return 0;
    }
    strcpy(name, buf);
    return 1;
}

static int input_age(int *age) {
    long a;
    if (!read_long("Enter age (16-100): ", &a) || !is_valid_age(a)) {
        printf("Error: age must be an integer between 16 and 100.\n");
        return 0;
    }
    *age = (int)a;
    return 1;
}

static int input_gpa(float *gpa) {
    double g;
    if (!read_double("Enter GPA (0.00-4.00): ", &g) || !is_valid_gpa(g)) {
        printf("Error: GPA must be a number between 0.00 and 4.00.\n");
        return 0;
    }
    *gpa = (float)g;
    return 1;
}

/* ---------- Allocation / memory management ---------- */

/* Allocates an array of n students. Returns NULL (with a message) on any problem. */
Student* create_student_list(long n) {
    if (n <= 0) {
        printf("Error: capacity must be a positive number.\n");
        return NULL;
    }
    if (n > MAX_CAPACITY) {
        printf("Error: capacity too large (maximum %d).\n", MAX_CAPACITY);
        return NULL;
    }
    if ((size_t)n > SIZE_MAX / sizeof(Student)) {          /* overflow check */
        printf("Error: requested size overflows.\n");
        return NULL;
    }
    Student *studs = (Student*) malloc((size_t)n * sizeof(Student));
    if (studs == NULL) {
        printf("Error: memory allocation failed.\n");
    }
    return studs;
}

/* Doubles capacity using realloc. Original pointer is kept if realloc fails. */
static int grow_list(StudentList *list) {
    if (list->capacity >= MAX_CAPACITY) {
        printf("Error: maximum capacity reached.\n");
        return 0;
    }
    long new_cap = (list->capacity > MAX_CAPACITY / 2) ? MAX_CAPACITY
                                                       : (long)list->capacity * 2;
    if ((size_t)new_cap > SIZE_MAX / sizeof(Student)) {    /* overflow check */
        printf("Error: resize would overflow.\n");
        return 0;
    }
    Student *tmp = (Student *)realloc(list->data, (size_t)new_cap * sizeof(Student));
    if (tmp == NULL) {
        printf("Error: could not expand memory. Existing records are safe.\n");
        return 0;
    }
    list->data = tmp;
    list->capacity = (int)new_cap;
    printf("Info: capacity increased to %d.\n", list->capacity);
    return 1;
}

void free_student_list(StudentList *list) {
    free(list->data);
    list->data = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ---------- Record management ---------- */

static int find_index(const StudentList *list, int roll) {
    for (int i = 0; i < list->count; i++)
        if (list->data[i].roll == roll) return i;
    return -1;
}

static void print_student(const Student *s) {
    printf("Roll: %d | Name: %s | Age: %d | GPA: %.2f\n", s->roll, s->name, s->age, s->gpa);
}

/* Inserts one record (details already validated). Grows the array if full. */
int add_student(StudentList *list, int roll, const char *name, int age, float gpa) {
    if (find_index(list, roll) != -1) {
        printf("Error: roll number %d already exists.\n", roll);
        return 0;
    }
    if (list->count == list->capacity && !grow_list(list)) return 0;

    Student *s = &list->data[list->count++];
    s->roll = roll;
    strcpy(s->name, name);
    s->age = age;
    s->gpa = gpa;
    printf("Success: student added.\n");
    return 1;
}

/* Menu option: reads details from the user, then calls add_student */
static void add_student_menu(StudentList *list) {
    int roll, age;
    float gpa;
    char name[NAME_SIZE];

    if (!input_roll(&roll)) return;
    if (find_index(list, roll) != -1) {
        printf("Error: roll number %d already exists.\n", roll);
        return;
    }
    if (!input_name(name) || !input_age(&age) || !input_gpa(&gpa)) return;

    add_student(list, roll, name, age, gpa);
}

void display_students(const StudentList *list) {
    if (list->count == 0) {
        printf("No records to display.\n");
        return;
    }
    for (int i = 0; i < list->count; i++) print_student(&list->data[i]);
}

static void search_student(const StudentList *list) {
    int roll;
    if (!input_roll(&roll)) return;
    int i = find_index(list, roll);
    if (i == -1) printf("Error: roll number %d not found.\n", roll);
    else print_student(&list->data[i]);
}

static void update_student(StudentList *list) {
    int roll, age;
    float gpa;
    char name[NAME_SIZE];

    if (!input_roll(&roll)) return;
    int i = find_index(list, roll);
    if (i == -1) {
        printf("Error: roll number %d not found.\n", roll);
        return;
    }
    printf("Enter new details (roll number stays %d).\n", roll);
    if (!input_name(name) || !input_age(&age) || !input_gpa(&gpa)) {
        printf("Update cancelled, record unchanged.\n");
        return;
    }
    strcpy(list->data[i].name, name);
    list->data[i].age = age;
    list->data[i].gpa = gpa;
    printf("Success: record updated.\n");
}

static void delete_student(StudentList *list) {
    int roll;
    if (!input_roll(&roll)) return;
    int i = find_index(list, roll);
    if (i == -1) {
        printf("Error: roll number %d not found.\n", roll);
        return;
    }
    /* shift later records left so all other records are preserved */
    memmove(&list->data[i], &list->data[i + 1],
            (size_t)(list->count - i - 1) * sizeof(Student));
    list->count--;
    printf("Success: record deleted.\n");
}

/* ---------- Analytics ---------- */

static void gpa_summary(const StudentList *list) {
    if (list->count == 0) {
        printf("No records available, so GPA summary cannot be computed.\n");
        return;
    }
    const Student *hi = &list->data[0], *lo = &list->data[0];
    double sum = 0.0;
    for (int i = 0; i < list->count; i++) {
        if (list->data[i].gpa > hi->gpa) hi = &list->data[i];
        if (list->data[i].gpa < lo->gpa) lo = &list->data[i];
        sum += list->data[i].gpa;
    }
    printf("Highest GPA: "); print_student(hi);
    printf("Lowest GPA : "); print_student(lo);
    printf("Average GPA: %.2f\n", sum / list->count);
}

/* GPA descending, ties broken by roll number ascending */
static int compare_students(const void *a, const void *b) {
    const Student *x = (const Student *)a, *y = (const Student *)b;
    if (x->gpa < y->gpa) return 1;
    if (x->gpa > y->gpa) return -1;
    return (x->roll > y->roll) - (x->roll < y->roll);
}

/* Design choice: sorting is done on a COPY, so the original insertion order is preserved. */
static void display_sorted(const StudentList *list) {
    if (list->count == 0) {
        printf("No records to sort.\n");
        return;
    }
    Student *copy = (Student *)malloc((size_t)list->count * sizeof(Student));
    if (copy == NULL) {
        printf("Error: not enough memory to sort.\n");
        return;
    }
    memcpy(copy, list->data, (size_t)list->count * sizeof(Student));
    qsort(copy, (size_t)list->count, sizeof(Student), compare_students);
    for (int i = 0; i < list->count; i++) print_student(&copy[i]);
    free(copy);
}

/* ---------- Menu ---------- */

static void print_menu(void) {
    printf("\n===== Student Records System =====\n");
    printf("1. Add student\n2. Display all\n3. Search by roll\n4. Update student\n");
    printf("5. Delete student\n6. GPA summary\n7. Display sorted by GPA\n0. Exit\n");
}

int main(void) {
    StudentList list = { NULL, 0, 0 };
    long cap;

    while (1) {
        if (!read_long("Enter initial capacity: ", &cap)) {
            if (feof(stdin)) return 1;
            printf("Error: please enter a valid integer.\n");
            continue;
        }
        list.data = create_student_list(cap);
        if (list.data != NULL) {
            list.capacity = (int)cap;
            list.count = 0;
            break;
        }
    }

    int running = 1;
    while (running) {
        long choice;
        print_menu();
        if (!read_long("Enter choice: ", &choice)) {
            if (feof(stdin)) break;
            printf("Error: invalid choice.\n");
            continue;
        }
        switch (choice) {
            case 1: add_student_menu(&list);  break;
            case 2: display_students(&list);  break;
            case 3: search_student(&list);    break;
            case 4: update_student(&list);    break;
            case 5: delete_student(&list);    break;
            case 6: gpa_summary(&list);       break;
            case 7: display_sorted(&list);    break;
            case 0: running = 0;              break;
            default: printf("Error: choose a number from 0 to 7.\n");
        }
    }

    free_student_list(&list);
    printf("Goodbye!\n");
    return 0;
}