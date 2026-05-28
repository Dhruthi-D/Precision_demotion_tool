#include <iostream>

int main() {
    double x = 1.2345f;
    double y = 2.3456f;
    double z = 3.4567f;

    double r =
        x * y +
        y * z +
        x * z +
        0.125f;

    std::cout << r;
    return 0;
}