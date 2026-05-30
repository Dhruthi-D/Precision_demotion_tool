#include <iostream>

int main() {
    float x0 = 1.20f;
    float x1 = -0.40f;
    float x2 = 0.70f;
    float x3 = 2.10f;

    float mean = 0.90f;
    float invStd = 0.625f;

    float gamma0 = 1.05f;
    float gamma1 = 0.95f;
    float gamma2 = 1.10f;
    float gamma3 = 0.85f;

    float beta0 = 0.10f;
    float beta1 = -0.05f;
    float beta2 = 0.02f;
    float beta3 = 0.08f;

    float n0 = (x0 - mean) * invStd;
    float n1 = (x1 - mean) * invStd;
    float n2 = (x2 - mean) * invStd;
    float n3 = (x3 - mean) * invStd;

    float y0 = n0 * gamma0 + beta0;
    float y1 = n1 * gamma1 + beta1;
    float y2 = n2 * gamma2 + beta2;
    float y3 = n3 * gamma3 + beta3;

    float output = y0 + y1 + y2 + y3;

    std::cout << output;

    return 0;
}
