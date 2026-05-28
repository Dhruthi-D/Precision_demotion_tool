#include <iostream>

int main() {
    float x = 1.2345f;
    float y = 2.3456f;
    float z = 3.4567f;

    float r =
        x * y +
        y * z +
        x * z +
        0.125f;

    std::cout << r;
    return 0;
}