#include <iostream>

int main() {
    __fp16 x0 = 0.25f;
    __fp16 x1 = -1.50f;
    __fp16 x2 = 2.00f;
    __fp16 x3 = 0.75f;

    __fp16 w0 = 1.20f;
    __fp16 w1 = -0.40f;
    __fp16 w2 = 0.85f;
    __fp16 w3 = 2.10f;

    __fp16 bias = -0.35f;

    __fp16 y0 = x0 * w0;
    __fp16 y1 = x1 * w1;
    __fp16 y2 = x2 * w2;
    __fp16 y3 = x3 * w3;

    __fp16 output = y0 + y1 + y2 + y3 + bias;

    std::cout << output;

    return 0;
}
