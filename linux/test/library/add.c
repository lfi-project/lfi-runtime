int
add(int a, int b)
{
    return a + b;
}

static __thread int tls_count;

// Returns how many times the calling thread has called this. Only callable
// from threads that have TLS (see test_detach.c).
int
tls_incr(void)
{
    return ++tls_count;
}
