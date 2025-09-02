#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <random>
#include <chrono>
#include <queue>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::io_context;
using boost::asio::steady_timer;
using namespace std::chrono_literals;


std::mt19937 randomGenerator(std::random_device{}());
std::uniform_real_distribution<> distribution(0, 1);

awaitable<bool> simulateApiCall(int id) {
    int delayMs = 1000;

    steady_timer timer(co_await boost::asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(delayMs));
    co_await timer.async_wait(boost::asio::use_awaitable);

    std::cout << "RUN - Task[" << id << "] finished after " << delayMs << "ms.\n";
    co_return distribution(randomGenerator) < 0.8;
}

awaitable<void> callApi(int id) {
    int tries = 0;
    while (true) {
        ++tries;
        if (co_await simulateApiCall(id)) {
            std::cout << "SUCCESS - Task[" << id << "] after " << tries << " tries.\n";
            co_return;
        } else {
            std::cout << "FAIL - Task[" << id << "] failed.\n";
        }
    }
}

int runWithQueueDispatcher(int totalTasks, int maxConnections) {
    io_context ctx;

    auto start = std::chrono::high_resolution_clock::now();

    std::queue<int> tasks;
    for(int i = 0; i < totalTasks; i++) {
        tasks.push(i);
    }

    int running = 0;
    boost::asio::steady_timer allDone(ctx);

    std::function<awaitable<void>(int)> launch = [&](int id) -> awaitable<void> {
        ++running;
        co_await callApi(id);
        --running;

        if(!tasks.empty()) {
            int next = tasks.front();
            tasks.pop();
            co_spawn(ctx, launch(next), detached);
        }

        if(running == 0 && tasks.empty()) {
            allDone.cancel();
        }

        co_return;
    };

    for(int i = 0; i < maxConnections; i++) {
        int id = tasks.front();
        tasks.pop();
        co_spawn(ctx, launch(id), detached);
    }

    allDone.expires_at(std::chrono::steady_clock::time_point::max());
    allDone.async_wait(boost::asio::use_awaitable);
    ctx.run();

    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
}

int main() {
    int taskCount = 100;
    int maxConnections = 5;

    std::cout << "START - Queue Dispatcher started..." << std::endl;
    int timeQueueDispatcher = runWithQueueDispatcher(taskCount, maxConnections);

    std::cout << "END - Total time - Queue Dispatcher: " << timeQueueDispatcher << " ms.\n";
}
