#include <iostream>

int main() {
    float image0 = 0.10f;
    float image1 = 0.80f;
    float image2 = -0.30f;
    float image3 = 0.45f;

    float k0 = 0.25f;
    float k1 = -0.50f;
    float k2 = 0.75f;
    float k3 = 1.10f;

    float bias = 0.05f;

    float conv0 = image0 * k0;
    float conv1 = image1 * k1;
    float conv2 = image2 * k2;
    float conv3 = image3 * k3;

    float sum = conv0 + conv1 + conv2 + conv3 + bias;
    float relu = sum > 0.0f ? sum : 0.0f;

    std::cout << relu;

    return 0;
}
