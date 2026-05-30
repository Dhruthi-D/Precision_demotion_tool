#include <iostream>

int main() {
    float i0 = 0.80f;
    float q0 = -0.20f;
    float i1 = 0.30f;
    float q1 = 0.60f;
    float i2 = -0.50f;
    float q2 = 0.40f;
    float i3 = 0.10f;
    float q3 = -0.70f;

    float p0 = i0 * i0 + q0 * q0;
    float p1 = i1 * i1 + q1 * q1;
    float p2 = i2 * i2 + q2 * q2;
    float p3 = i3 * i3 + q3 * q3;

    float averagePower = (p0 + p1 + p2 + p3) * 0.25f;
    float gain = 1.50f;
    float output = averagePower * gain;

    std::cout << output;

    return 0;
}
