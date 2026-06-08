#include <mutex>
#include <thread>

std::mutex m;

int main() {
    std::thread t([](){});
    t.join();
}