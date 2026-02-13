#include "DelegateWorker.h"
#include <iostream>
#include <pthread.h>
#include <sched.h>

DelegateWorker::DelegateWorker() {
    // Il thread verrà creato in start()
}

DelegateWorker::~DelegateWorker() {
    stop();
}

void DelegateWorker::start() {
    if (!running) {
        running = true;
        worker_thread = std::thread(&DelegateWorker::workerFunction, this);
        
        // Imposta priorità bassa per il thread del delegate
        struct sched_param worker_param;
        worker_param.sched_priority = sched_get_priority_min(SCHED_OTHER);
        if (pthread_setschedparam(worker_thread.native_handle(), SCHED_OTHER, &worker_param)) {
            std::cerr << "Warning: Could not set low priority for delegate worker thread" << std::endl;
        }
    }
}

void DelegateWorker::stop() {
    if (running) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            running = false;
        }
        queue_cv.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
}

void DelegateWorker::queueTask(std::function<void()> task) {
    if (running) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(std::move(task));
        }
        queue_cv.notify_one();
    }
}

void DelegateWorker::workerFunction() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !task_queue.empty() || !running;
            });
            
            if (!running && task_queue.empty())
                break;
                
            if (!task_queue.empty()) {
                task = std::move(task_queue.front());
                task_queue.pop();
            }
        }
        
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Exception in delegate task: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in delegate task" << std::endl;
            }
        }
    }
}