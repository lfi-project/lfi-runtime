#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int add(int a, int b);
void get_value(int* out);
int double_value(const int* in);
void increment(int* inout);

#ifdef __cplusplus
}
#endif
