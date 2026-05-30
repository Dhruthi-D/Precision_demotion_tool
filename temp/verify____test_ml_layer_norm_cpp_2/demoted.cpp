#include <iostream>

int main() {
    __fp16 x0 = 1.20f;
    float x1 = -0.40f;
    __fp16 x2 = 0.70f;
    __fp16 x3 = 2.10f;

    __fp16 mean = 0.90f;
    __fp16 invStd = 0.625f;

    __fp16 gamma0 = 1.05f;
    __fp16 gamma1 = 0.95f;
    __fp16 gamma2 = 1.10f;
    __fp16 gamma3 = 0.85f;

    __fp16 beta0 = 0.10f;
    __fp16 beta1 = -0.05f;
    __fp16 beta2 = 0.02f;
    __fp16 beta3 = 0.08f;

    __bf16 n0 = (x0 - mean) * invStd;
    __bf16 n1 = (x1 - mean) * invStd;
    __bf16 n2 = (x2 - mean) * invStd;
    __bf16 n3 = (x3 - mean) * invStd;

    __fp16 y0 = n0 * gamma0 + beta0;
    __fp16 y1 = n1 * gamma1 + beta1;
    __fp16 y2 = n2 * gamma2 + beta2;
    __fp16 y3 = n3 * gamma3 + beta3;

    __fp16 output = y0 + y1 + y2 + y3;

    std::cout << output;

    return 0;
}
