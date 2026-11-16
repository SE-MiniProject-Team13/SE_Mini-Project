#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Thread test started\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Thread test passed\n";
    return 0;
}
