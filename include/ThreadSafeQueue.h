#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

//  http://mezhenskyi.dev/posts/task-queue/
template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool is_done = false;

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(std::move(value));
        cv.notify_one(); 
    }
    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        
        cv.wait(lock, [this] { return !queue.empty() || is_done; });
        
        if (queue.empty() && is_done) {
            return false;
        }
        
        value = std::move(queue.front());
        queue.pop();
        return true;
    }

    void set_done() {
        std::lock_guard<std::mutex> lock(mtx);
        is_done = true;
        cv.notify_all();
    }
};