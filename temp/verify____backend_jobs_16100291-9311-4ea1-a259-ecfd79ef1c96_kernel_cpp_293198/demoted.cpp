float filter(float x) {
    __fp16 y =
        x * 0.5f
        + x * x * 0.25f;

    return y;
}

#include <iostream>

int main() {
    std::cout << filter(2.1f);
    return 0;
}