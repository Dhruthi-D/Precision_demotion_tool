#include <iostream>

float scale(float x) {
    float y =
        x * 0.5f
        + 0.0001f;

    return y;
}

int main() {
    std::cout << scale(0.01f);
    return 0;
}
