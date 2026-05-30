#include <iostream>

int main() {
    float x0 = 0.20f;
    float x1 = 0.40f;
    float x2 = 0.10f;
    float x3 = -0.30f;
    float x4 = 0.70f;

    float h0 = 0.10f;
    float h1 = 0.15f;
    float h2 = 0.50f;
    float h3 = 0.15f;
    float h4 = 0.10f;

    float tap0 = x0 * h0;
    float tap1 = x1 * h1;
    float tap2 = x2 * h2;
    float tap3 = x3 * h3;
    float tap4 = x4 * h4;

    float y = tap0 + tap1 + tap2 + tap3 + tap4;

    std::cout << y;

    return 0;
}
