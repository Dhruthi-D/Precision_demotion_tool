#include <iostream>
#include <cmath>

// Simulate FP16
float toFP16(float x) {
    return round(x * 1000.0f) / 1000.0f;
}

// Kernel 1
void kernel1() {
    float a = 2.1f, b = 3.2f;
    float fp32 = a * b;
    float fp16 = toFP16(a) * toFP16(b);

    std::cout << "\nKernel1 (Multiply)\n";
    std::cout << "FP32: " << fp32 << "\n";
    std::cout << "FP16: " << fp16 << "\n";
    std::cout << "Error: " << fabs(fp32 - fp16) << "\n";
}

// Kernel 2
void kernel2() {
    float sum = 0;
    for(int i=1;i<=5;i++)
        sum += i*i;

    float fp16 = toFP16(sum);

    std::cout << "\nKernel2 (Dot-like)\n";
    std::cout << "FP32: " << sum << "\n";
    std::cout << "FP16: " << fp16 << "\n";
    std::cout << "Error: " << fabs(sum - fp16) << "\n";
}

// Kernel 3
void kernel3() {
    float x = 2.12345;
    float fp32 = x*x*x + 2*x*x + x;
    float fp16 = toFP16(x)*toFP16(x)*toFP16(x);

    std::cout << "\nKernel3 (Polynomial)\n";
    std::cout << "FP32: " << fp32 << "\n";
    std::cout << "FP16: " << fp16 << "\n";
    std::cout << "Error: " << fabs(fp32 - fp16) << "\n";
}

// Kernel 4
void kernel4() {
    float x = 2.1;
    float fp32 = x*0.5 + x*x*0.25;
    float fp16 = toFP16(x)*0.5 + toFP16(x)*toFP16(x)*0.25;

    std::cout << "\nKernel4 (Filter)\n";
    std::cout << "FP32: " << fp32 << "\n";
    std::cout << "FP16: " << fp16 << "\n";
    std::cout << "Error: " << fabs(fp32 - fp16) << "\n";
}

// Kernel 5
void kernel5() {
    float x=2.1, mean=1.5, std=0.7;
    float fp32 = (x-mean)/std;
    float fp16 = (toFP16(x)-toFP16(mean))/toFP16(std);

    std::cout << "\nKernel5 (Normalize)\n";
    std::cout << "FP32: " << fp32 << "\n";
    std::cout << "FP16: " << fp16 << "\n";
    std::cout << "Error: " << fabs(fp32 - fp16) << "\n";
}

int main() {
    kernel1();
    kernel2();
    kernel3();
    kernel4();
    kernel5();
}