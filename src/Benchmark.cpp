#include <chrono>

double measureTime(
    void (*func)()) {

    auto start =
        std::chrono
        ::high_resolution_clock
        ::now();

    func();

    auto end =
        std::chrono
        ::high_resolution_clock
        ::now();

    return
        std::chrono
        ::duration<double,
        std::micro>
        (end-start).count();
}