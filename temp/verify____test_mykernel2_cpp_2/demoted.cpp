#include <iostream>

float custom(float x, float y) {

    __fp16 z = x*y + x/y;

    return z+z;
}

int main() {

    __fp16 result =
        custom(4.8f, 2.1f);

    std::cout << result;

    return 0;
}