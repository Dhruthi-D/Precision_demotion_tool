#include <iostream>

int main() {
    float x0 = 0.80f;
    float x1 = 0.35f;
    float x2 = -0.20f;
    float y1 = 0.10f;
    float y2 = -0.05f;

    float b0 = 0.2929f;
    float b1 = 0.5858f;
    float b2 = 0.2929f;
    float a1 = -0.0000f;
    float a2 = 0.1716f;

    float ff0 = b0 * x0;
    float ff1 = b1 * x1;
    float ff2 = b2 * x2;
    float fb1 = a1 * y1;
    float fb2 = a2 * y2;

    float y0 = ff0 + ff1 + ff2 - fb1 - fb2;

    std::cout << y0;

    return 0;
}
