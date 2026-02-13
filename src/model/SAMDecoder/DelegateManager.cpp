#include "DelegateManager.h"
#include "SAMDecoder.h" // Per SAMDecoderDelegate
#include <algorithm>
#include <typeinfo>

DelegateManager::DelegateManager() {
}

DelegateManager::~DelegateManager() {
    clearDelegates();
}

void DelegateManager::createWorkerForDelegate(SAMDecoderDelegate* delegate) {
    if (!isMultithreaded) return;

    auto worker = std::make_unique<DelegateWorker>();
    worker->start();
    delegate_workers[delegate] = std::move(worker);
}

void DelegateManager::removeWorkerForDelegate(SAMDecoderDelegate* delegate) {
    auto it = delegate_workers.find(delegate);
    if (it != delegate_workers.end()) {
        it->second->stop();
        delegate_workers.erase(it);
    }
}

void DelegateManager::addDelegate(SAMDecoderDelegate* delegate) {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    if (delegate && std::find(delegates.begin(), delegates.end(), delegate) == delegates.end()) {
        delegates.push_back(delegate);
        createWorkerForDelegate(delegate);
    }
}

void DelegateManager::removeDelegate(SAMDecoderDelegate* delegate) {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    delegates.erase(std::remove(delegates.begin(), delegates.end(), delegate), delegates.end());
    removeWorkerForDelegate(delegate);
}

void DelegateManager::removeDelegateOfType(const std::type_info& type) {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    
    auto it = delegates.begin();
    while (it != delegates.end()) {
        if (typeid(**it) == type) {
            removeWorkerForDelegate(*it);
            it = delegates.erase(it);
        } else {
            ++it;
        }
    }
}

void DelegateManager::clearDelegates() {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    for (auto* delegate : delegates) {
        removeWorkerForDelegate(delegate);
    }
    delegates.clear();
}

void DelegateManager::willStopAcquisition() {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    for (auto* delegate : delegates) {
        delegate->acquisitionWillStop();
    }
}

void DelegateManager::queueTaskForDelegate(SAMDecoderDelegate* delegate, std::function<void()> task) {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    if (!isMultithreaded) {
        // If multithreading is disabled, execute the task directly
        task();
        return;
    }
    auto it = delegate_workers.find(delegate);
    if (it != delegate_workers.end()) {
        it->second->queueTask(std::move(task));
    }
}

void DelegateManager::queueTaskForAllDelegates(std::function<void(SAMDecoderDelegate*)> taskFactory) {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    for (auto* delegate : delegates) {
        if (!isMultithreaded) {
            // If multithreading is disabled, execute the task directly
            taskFactory(delegate);
            continue;
        }
        auto it = delegate_workers.find(delegate);
        if (it != delegate_workers.end()) {
            it->second->queueTask([taskFactory, delegate]() {
                taskFactory(delegate);
            });
        }
    }
}



std::vector<SAMDecoderDelegate*> DelegateManager::getDelegatesCopy() const {
    std::lock_guard<std::mutex> lock(delegates_mutex);
    return delegates;
}