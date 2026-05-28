#include <iostream>

float custom(float x, float y) {

    float z = x*y + x/y;

    return z+z;
}

int main() {

    float result =
        custom(4.8f, 2.1f);

    std::cout << result;

    return 0;
}