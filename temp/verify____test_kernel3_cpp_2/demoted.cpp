float poly(float x) {

    __fp16 y =
        x*x*x
        + 2*x*x
        + x + 1;

    return y;
}

#include <iostream>

int main() {

    std::cout << poly(2.12345f);

    return 0;
}
