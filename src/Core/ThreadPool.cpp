#include "ThreadPool.h"
#include "Logger.h"

ThreadPool::ThreadPool(size_t numThreads) : stop(false), activeTasks(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                
                task();
                
                activeTasks--;
                completionCondition.notify_all();
            }
        });
    }
    
    LOG_INFO("ThreadPool initialized with " + std::to_string(numThreads) + " threads");
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    LOG_INFO("ThreadPool destroyed");
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(queueMutex);
    completionCondition.wait(lock, [this] {
        return tasks.empty() && activeTasks == 0;
    });
}
