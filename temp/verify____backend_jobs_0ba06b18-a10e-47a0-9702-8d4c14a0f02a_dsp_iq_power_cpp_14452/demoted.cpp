#include <iostream>

int main() {
    __fp16 i0 = 0.80f;
    __bf16 q0 = -0.20f;
    __fp16 i1 = 0.30f;
    __fp16 q1 = 0.60f;
    __bf16 i2 = -0.50f;
    __fp16 q2 = 0.40f;
    __fp16 i3 = 0.10f;
    __bf16 q3 = -0.70f;

    __bf16 p0 = i0 * i0 + q0 * q0;
    __bf16 p1 = i1 * i1 + q1 * q1;
    __bf16 p2 = i2 * i2 + q2 * q2;
    __bf16 p3 = i3 * i3 + q3 * q3;

    __fp16 averagePower = (p0 + p1 + p2 + p3) * 0.25f;
    __fp16 gain = 1.50f;
    __fp16 output = averagePower * gain;

    std::cout << output;

    return 0;
}
