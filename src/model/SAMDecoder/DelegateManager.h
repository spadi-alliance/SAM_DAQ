#pragma once
#include "DelegateWorker.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>

// Forward declaration
class SAMDecoderDelegate;

class DelegateManager {
public:
    DelegateManager();
    ~DelegateManager();

    void addDelegate(SAMDecoderDelegate* delegate);
    void removeDelegate(SAMDecoderDelegate* delegate);
    void removeDelegateOfType(const std::type_info& type);
    void clearDelegates();
    
    void queueTaskForDelegate(SAMDecoderDelegate* delegate, std::function<void()> task);
    void queueTaskForAllDelegates(std::function<void(SAMDecoderDelegate*)> taskFactory);
    
    std::vector<SAMDecoderDelegate*> getDelegatesCopy() const;
    
    void willStopAcquisition();

    bool isMultithreaded = true; // Flag to enable/disable multithreading

private:
    std::vector<SAMDecoderDelegate*> delegates;
    mutable std::mutex delegates_mutex;
    
    // Map: delegate -> worker thread dedicato
    std::unordered_map<SAMDecoderDelegate*, std::unique_ptr<DelegateWorker>> delegate_workers;
    
    void createWorkerForDelegate(SAMDecoderDelegate* delegate);
    void removeWorkerForDelegate(SAMDecoderDelegate* delegate);
};