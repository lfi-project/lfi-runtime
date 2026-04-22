// Functions demonstrating pointer parameter patterns

// Out parameter: writes result to pointer
void get_value(int* out) {
    *out = 42;
}

// In parameter: reads from pointer
int double_value(const int* in) {
    return (*in) * 2;
}

// InOut parameter: reads, modifies, writes back
void increment(int* inout) {
    (*inout)++;
}

// Multiple parameters
void add_to_result(const int* a, const int* b, int* result) {
    *result = *a + *b;
}

// Struct example
struct Point {
    int x;
    int y;
};

void translate_point(struct Point* p, int dx, int dy) {
    p->x += dx;
    p->y += dy;
}

void get_origin(struct Point* out) {
    out->x = 0;
    out->y = 0;
}

int manhattan_distance(const struct Point* p) {
    int x = p->x < 0 ? -p->x : p->x;
    int y = p->y < 0 ? -p->y : p->y;
    return x + y;
}
