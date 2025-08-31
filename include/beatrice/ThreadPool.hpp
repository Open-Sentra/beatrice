#ifndef BEATRICE_THREAD_POOL_HPP
#define BEATRICE_THREAD_POOL_HPP

#include "Error.hpp"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace beatrice {

class ThreadPool {
public:
    struct ThreadConfig {
        int threadId;
        int cpuId;
        std::string name;
        size_t maxTasks;
        bool pinToCore;
    };

    struct TaskStats {
        uint64_t tasksSubmitted = 0;
        uint64_t tasksCompleted = 0;
        uint64_t tasksFailed = 0;
        std::chrono::microseconds totalProcessingTime{0};
        std::chrono::microseconds averageProcessingTime{0};
        std::chrono::microseconds maxProcessingTime{0};
        std::chrono::microseconds minProcessingTime{std::chrono::microseconds::max()};
    };

    struct LoadBalancingConfig {
        enum class Strategy {
            ROUND_ROBIN,
            LEAST_LOADED,
            WEIGHTED_ROUND_ROBIN,
            ADAPTIVE
        };
        
        Strategy strategy = Strategy::ROUND_ROBIN;
        bool enableAdaptive = true;
        size_t adaptiveThreshold = 1000;
        std::vector<double> threadWeights;
    };

    ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    void setThreadAffinity(int threadId, int cpuId);
    void enableLoadBalancing(const LoadBalancingConfig& config);
    void setThreadPriority(int threadId, int priority);
    void setMaxTasksPerThread(size_t maxTasks);
    
    size_t getActiveThreadCount() const;
    size_t getPendingTaskCount() const;
    TaskStats getThreadStats(int threadId) const;
    TaskStats getOverallStats() const;
    
    void pause();
    void resume();
    void shutdown();
    bool isShutdown() const;

private:
    struct ThreadInfo {
        std::thread thread;
        int cpuId;
        std::string name;
        size_t maxTasks;
        std::queue<std::function<void()>> localQueue;
        std::mutex queueMutex;
        std::condition_variable condition;
        std::atomic<bool> running{true};
        TaskStats stats;
        std::atomic<size_t> currentLoad{0};
    };

    struct Task {
        std::function<void()> func;
        int priority;
        std::chrono::steady_clock::time_point submitTime;
        std::string description;
    };

    std::vector<std::unique_ptr<ThreadInfo>> threads_;
    std::queue<Task> globalQueue_;
    std::mutex globalQueueMutex_;
    std::condition_variable globalCondition_;
    
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> paused_{false};
    std::atomic<size_t> nextThread_{0};
    
    LoadBalancingConfig loadBalancingConfig_;
    mutable std::mutex statsMutex_;

    void workerFunction(size_t threadId);
    size_t selectThread(const Task& task);
    size_t roundRobinSelect();
    size_t leastLoadedSelect();
    size_t weightedRoundRobinSelect();
    size_t adaptiveSelect();
    
    void updateThreadStats(size_t threadId, const std::chrono::microseconds& processingTime, bool success);
    void balanceLoad();
    bool stealTasks(size_t thiefId, size_t victimId);
    
    void pinThreadToCore(std::thread& thread, int cpuId);
    void setThreadPriority(std::thread& thread, int priority);
};

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using ReturnType = typename std::result_of<F(Args...)>::type;
    
    if (shutdown_.load()) {
        throw std::runtime_error("ThreadPool is shutdown");
    }
    
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<ReturnType> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(globalQueueMutex_);
        globalQueue_.push({[task]() { (*task)(); }, 0, std::chrono::steady_clock::now(), "User task"});
    }
    
    globalCondition_.notify_one();
    return result;
}

} // namespace beatrice

#endif // BEATRICE_THREAD_POOL_HPP 