typedef int (*add_fn_t)(int a, int b);

int process_data(int value, add_fn_t add) {
    // Call the callback - transparently invokes the host function
    int ret1 = add(value, 10);

    // Do some "work"
    int result = value * 2;

    // Call callback again
    int ret2 = add(result, 5);

    // Return something that proves callbacks worked
    return ret1 + ret2;
}
