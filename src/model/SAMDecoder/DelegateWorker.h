#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>

class DelegateWorker {
public:
    DelegateWorker();
    ~DelegateWorker();

    void start();
    void stop();
    void queueTask(std::function<void()> task);
    bool isRunning() const { return running; }

private:
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::thread worker_thread;
    std::atomic<bool> running{false};
    
    void workerFunction();
};