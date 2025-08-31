#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <semaphore>
#include <thread>
#include <coroutine>

using namespace std::chrono;

// Awaitable sleep
struct AsyncSleep {
    milliseconds duration;
    bool await_ready() const noexcept { return duration.count() <= 0; }
    void await_suspend(std::coroutine_handle<> h) const {
        std::thread([h, d = duration](){
            std::this_thread::sleep_for(d);
            h.resume();
        }).detach();
    }
    void await_resume() const noexcept {}
};

// Minimal async task
struct Task {
    struct promise_type {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> coro;
    Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    ~Task() { if(coro) coro.destroy(); }
};

// Semaphore for concurrency
std::counting_semaphore<> semaphore(5);

Task asyncApiCall(int id) {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<> dist(0,1);

    co_await AsyncSleep{milliseconds(100)};

    bool success = dist(rng) < 0.8;
    std::cout << "RUN - Task[" << id << "] finished. Success: " << success << "\n";
}

Task callApiAsync(int id) {
    int tries = 0;
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<> dist(0,1);

    while(true) {
        ++tries;
        semaphore.acquire();
        co_await asyncApiCall(id);
        semaphore.release();

        if (dist(rng) < 0.8) {
            std::cout << "SUCCESS - Task[" << id << "] after " << tries << " tries.\n";
            co_return;
        } else {
            std::cout << "FAIL - Task[" << id << "] retrying...\n";
        }
    }
}

int main() {
    int taskCount = 20;
    std::vector<Task> tasks;

    for(int i = 0; i < taskCount; ++i) {
        tasks.push_back(callApiAsync(i));
    }

    // Let tasks finish
    std::this_thread::sleep_for(std::chrono::seconds(10));
}
