#include <cassert>
#include <cstdio>

#if defined(SBOX_LFI)
#include "sbox/lfi.h"
using SboxType = sbox::Sandbox<sbox::LFI>;
#else
#include "sbox/passthrough.h"
using SboxType = sbox::Sandbox<sbox::Passthrough>;
#endif

struct Point {
    int x;
    int y;
};

int main() {
#if defined(SBOX_LFI)
    auto sb = SboxType::create("./lib_inout.lfi");
    if (!sb) { fprintf(stderr, "failed to create sandbox\n"); return 1; }
    auto& sandbox = *sb;
    printf("Using lfi backend\n\n");
#else
    SboxType sandbox("./libinout.so");
    printf("Using passthrough backend\n\n");
#endif

    // Example 1: Out parameter
    {
        auto ctx = sandbox.context();
        int result;
        sandbox.call<void(int*)>(ctx, "get_value", ctx.out(result));
        printf("Out parameter: get_value() returned %d (expected 42)\n",
               result);
        assert(result == 42);
    }

    // Example 2: In parameter
    {
        auto ctx = sandbox.context();
        int value = 21;
        int result =
            sandbox.call<int(const int*)>(ctx, "double_value", ctx.in(value));
        printf("In parameter: double_value(%d) = %d (expected 42)\n", value,
               result);
        assert(result == 42);
    }

    // Example 3: InOut parameter
    {
        auto ctx = sandbox.context();
        int counter = 5;
        printf("InOut parameter: counter before = %d\n", counter);
        sandbox.call<void(int*)>(ctx, "increment", ctx.inout(counter));
        printf("InOut parameter: counter after increment = %d (expected 6)\n",
               counter);
        assert(counter == 6);
    }

    // Example 4: Multiple parameters
    {
        auto ctx = sandbox.context();
        int a = 10, b = 32, result;
        sandbox.call<void(const int*, const int*, int*)>(
            ctx, "add_to_result", ctx.in(a), ctx.in(b), ctx.out(result));
        printf("Multiple params: add_to_result(%d, %d) = %d (expected 42)\n", a,
               b, result);
        assert(result == 42);
    }

    // Example 5: Struct with inout
    {
        auto ctx = sandbox.context();
        Point p = {10, 20};
        printf("Struct inout: point before = (%d, %d)\n", p.x, p.y);
        sandbox.call<void(Point*, int, int)>(ctx, "translate_point",
                                             ctx.inout(p), 5, -10);
        printf(
            "Struct inout: point after translate(5, -10) = (%d, %d) (expected "
            "15, 10)\n",
            p.x, p.y);
        assert(p.x == 15 && p.y == 10);
    }

    // Example 6: Struct with out
    {
        auto ctx = sandbox.context();
        Point origin;
        sandbox.call<void(Point*)>(ctx, "get_origin", ctx.out(origin));
        printf("Struct out: get_origin() = (%d, %d) (expected 0, 0)\n",
               origin.x, origin.y);
        assert(origin.x == 0 && origin.y == 0);
    }

    // Example 7: Struct with in
    {
        auto ctx = sandbox.context();
        Point p = {-3, 4};
        int dist = sandbox.call<int(const Point*)>(ctx, "manhattan_distance",
                                                   ctx.in(p));
        printf("Struct in: manhattan_distance((%d, %d)) = %d (expected 7)\n",
               p.x, p.y, dist);
        assert(dist == 7);
    }

    printf("\nAll tests passed!\n");
    return 0;
}
