float normalize(
    float x,
    float mean,
    float std) {

    __fp16 y =
        (x-mean)/std;

    return y;
}

#include <iostream>

int main() {

    std::cout << normalize(2.1f, 1.5f, 0.7f);

    return 0;
}
