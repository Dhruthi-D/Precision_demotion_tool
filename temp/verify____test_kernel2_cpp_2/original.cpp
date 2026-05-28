float dot(
    float *a,
    float *b,
    int n) {

    float sum = 0;

    for(int i=0;i<n;i++)
        sum += a[i]*b[i];

    return sum;
}

#include <iostream>

int main() {

    float a[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float b[5] = {0.5f, 1.5f, 2.5f, 3.5f, 4.5f};

    std::cout << dot(a, b, 5);

    return 0;
}
