#include <iostream>

int main() {
    __fp16 x0 = 0.20f;
    __fp16 x1 = 0.40f;
    __fp16 x2 = 0.10f;
    __fp16 x3 = -0.30f;
    __fp16 x4 = 0.70f;

    __fp16 h0 = 0.10f;
    __fp16 h1 = 0.15f;
    __fp16 h2 = 0.50f;
    __fp16 h3 = 0.15f;
    __fp16 h4 = 0.10f;

    __fp16 tap0 = x0 * h0;
    __fp16 tap1 = x1 * h1;
    __fp16 tap2 = x2 * h2;
    __fp16 tap3 = x3 * h3;
    __fp16 tap4 = x4 * h4;

    __fp16 y = tap0 + tap1 + tap2 + tap3 + tap4;

    std::cout << y;

    return 0;
}
