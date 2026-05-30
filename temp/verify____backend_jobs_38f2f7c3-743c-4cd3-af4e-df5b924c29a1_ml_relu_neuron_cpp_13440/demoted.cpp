#include <iostream>

int main() {
    __fp16 image0 = 0.10f;
    __fp16 image1 = 0.80f;
    __fp16 image2 = -0.30f;
    __fp16 image3 = 0.45f;

    __fp16 k0 = 0.25f;
    __fp16 k1 = -0.50f;
    __fp16 k2 = 0.75f;
    __fp16 k3 = 1.10f;

    __fp16 bias = 0.05f;

    __fp16 conv0 = image0 * k0;
    __fp16 conv1 = image1 * k1;
    __fp16 conv2 = image2 * k2;
    __fp16 conv3 = image3 * k3;

    __fp16 sum = conv0 + conv1 + conv2 + conv3 + bias;
    __fp16 relu = sum > 0.0f ? sum : 0.0f;

    std::cout << relu;

    return 0;
}
