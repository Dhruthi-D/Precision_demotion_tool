#include <iostream>

int main() {
    float x0 = 0.25f;
    float x1 = -1.50f;
    float x2 = 2.00f;
    float x3 = 0.75f;

    float w0 = 1.20f;
    float w1 = -0.40f;
    float w2 = 0.85f;
    float w3 = 2.10f;

    float bias = -0.35f;

    float y0 = x0 * w0;
    float y1 = x1 * w1;
    float y2 = x2 * w2;
    float y3 = x3 * w3;

    float output = y0 + y1 + y2 + y3 + bias;

    std::cout << output;

    return 0;
}
