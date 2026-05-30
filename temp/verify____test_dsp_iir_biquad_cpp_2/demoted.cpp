#include <iostream>

int main() {
    __fp16 x0 = 0.80f;
    __fp16 x1 = 0.35f;
    __fp16 x2 = -0.20f;
    __fp16 y1 = 0.10f;
    __fp16 y2 = -0.05f;

    __fp16 b0 = 0.2929f;
    __fp16 b1 = 0.5858f;
    __fp16 b2 = 0.2929f;
    __fp16 a1 = -0.0000f;
    __fp16 a2 = 0.1716f;

    __fp16 ff0 = b0 * x0;
    __fp16 ff1 = b1 * x1;
    __fp16 ff2 = b2 * x2;
    __fp16 fb1 = a1 * y1;
    __fp16 fb2 = a2 * y2;

    __fp16 y0 = ff0 + ff1 + ff2 - fb1 - fb2;

    std::cout << y0;

    return 0;
}
