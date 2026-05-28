#include <iostream>

int main() {
    __fp16 x = 1.2345f;
    __fp16 y = 2.3456f;
    __fp16 z = 3.4567f;

    __fp16 r =
        x * y +
        y * z +
        x * z +
        0.125f;

    std::cout << r;
    return 0;
}