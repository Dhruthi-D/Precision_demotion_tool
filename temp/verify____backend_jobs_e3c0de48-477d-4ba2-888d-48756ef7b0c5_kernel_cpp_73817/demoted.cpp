#include <iostream>

int main() {
    __fp16 a = 1.25f;
    __fp16 b = 2.5f;
    __fp16 c = (a * b) + 0.125f;

    std::cout << c << std::endl;
    return 0;
}
