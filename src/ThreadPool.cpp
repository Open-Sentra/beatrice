#include "beatrice/ThreadPool.hpp"
#include "beatrice/Logger.hpp"
#include <algorithm>
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h>

namespace beatrice {

ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
    }
    
    threads_.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        auto threadInfo = std::make_unique<ThreadInfo>();
        threadInfo->cpuId = i % std::thread::hardware_concurrency();
        threadInfo->name = "Worker-" + std::to_string(i);
        threadInfo->maxTasks = 1000;
        
        threads_.push_back(std::move(threadInfo));
    }
    
    for (size_t i = 0; i < threads_.size(); ++i) {
        threads_[i]->thread = std::thread(&ThreadPool::workerFunction, this, i);
        
        if (loadBalancingConfig_.enableAdaptive) {
            setThreadAffinity(i, threads_[i]->cpuId);
        }
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::setThreadAffinity(int threadId, int cpuId) {
    if (threadId >= 0 && threadId < static_cast<int>(threads_.size())) {
        threads_[threadId]->cpuId = cpuId;
        pinThreadToCore(threads_[threadId]->thread, cpuId);
    }
}

void ThreadPool::enableLoadBalancing(const LoadBalancingConfig& config) {
    loadBalancingConfig_ = config;
    
    if (config.strategy == LoadBalancingConfig::Strategy::WEIGHTED_ROUND_ROBIN && 
        config.threadWeights.size() != threads_.size()) {
        loadBalancingConfig_.threadWeights.resize(threads_.size(), 1.0);
    }
}

void ThreadPool::setThreadPriority(int threadId, int priority) {
    if (threadId >= 0 && threadId < static_cast<int>(threads_.size())) {
        setThreadPriority(threads_[threadId]->thread, priority);
    }
}

void ThreadPool::setMaxTasksPerThread(size_t maxTasks) {
    for (auto& thread : threads_) {
        thread->maxTasks = maxTasks;
    }
}

size_t ThreadPool::getActiveThreadCount() const {
    size_t activeCount = 0;
    for (const auto& thread : threads_) {
        if (thread->running.load() && !thread->localQueue.empty()) {
            activeCount++;
        }
    }
    return activeCount;
}

size_t ThreadPool::getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(globalQueueMutex_));
    size_t totalPending = globalQueue_.size();
    
    for (const auto& thread : threads_) {
        std::lock_guard<std::mutex> threadLock(const_cast<std::mutex&>(thread->queueMutex));
        totalPending += thread->localQueue.size();
    }
    
    return totalPending;
}

ThreadPool::TaskStats ThreadPool::getThreadStats(int threadId) const {
    if (threadId >= 0 && threadId < static_cast<int>(threads_.size())) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return threads_[threadId]->stats;
    }
    return TaskStats{};
}

ThreadPool::TaskStats ThreadPool::getOverallStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    TaskStats overall;
    
    for (const auto& thread : threads_) {
        overall.tasksSubmitted += thread->stats.tasksSubmitted;
        overall.tasksCompleted += thread->stats.tasksCompleted;
        overall.tasksFailed += thread->stats.tasksFailed;
        overall.totalProcessingTime += thread->stats.totalProcessingTime;
        
        if (thread->stats.maxProcessingTime > overall.maxProcessingTime) {
            overall.maxProcessingTime = thread->stats.maxProcessingTime;
        }
        
        if (thread->stats.minProcessingTime < overall.minProcessingTime) {
            overall.minProcessingTime = thread->stats.minProcessingTime;
        }
    }
    
    if (overall.tasksCompleted > 0) {
        overall.averageProcessingTime = overall.totalProcessingTime / overall.tasksCompleted;
    }
    
    return overall;
}

void ThreadPool::pause() {
    paused_.store(true);
}

void ThreadPool::resume() {
    paused_.store(false);
    globalCondition_.notify_all();
    
    for (auto& thread : threads_) {
        thread->condition.notify_all();
    }
}

void ThreadPool::shutdown() {
    shutdown_.store(true);
    globalCondition_.notify_all();
    
    for (auto& thread : threads_) {
        thread->running.store(false);
        thread->condition.notify_all();
        
        if (thread->thread.joinable()) {
            thread->thread.join();
        }
    }
}

bool ThreadPool::isShutdown() const {
    return shutdown_.load();
}

void ThreadPool::workerFunction(size_t threadId) {
    auto& threadInfo = threads_[threadId];
    
    while (threadInfo->running.load()) {
        std::function<void()> task;
        bool hasTask = false;
        
        {
            std::unique_lock<std::mutex> lock(threadInfo->queueMutex);
            threadInfo->condition.wait(lock, [&]() {
                return !threadInfo->running.load() || !threadInfo->localQueue.empty();
            });
            
            if (!threadInfo->running.load()) break;
            
            if (!threadInfo->localQueue.empty()) {
                task = std::move(threadInfo->localQueue.front());
                threadInfo->localQueue.pop();
                hasTask = true;
            }
        }
        
        if (!hasTask) {
            std::unique_lock<std::mutex> globalLock(globalQueueMutex_);
            globalCondition_.wait(globalLock, [&]() {
                return !threadInfo->running.load() || !globalQueue_.empty();
            });
            
            if (!threadInfo->running.load()) break;
            
            if (!globalQueue_.empty()) {
                task = std::move(globalQueue_.front().func);
                globalQueue_.pop();
                hasTask = true;
            }
        }
        
        if (hasTask && !paused_.load()) {
            auto startTime = std::chrono::high_resolution_clock::now();
            threadInfo->currentLoad.fetch_add(1);
            
            try {
                task();
                auto endTime = std::chrono::high_resolution_clock::now();
                auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                updateThreadStats(threadId, processingTime, true);
            } catch (...) {
                auto endTime = std::chrono::high_resolution_clock::now();
                auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                updateThreadStats(threadId, processingTime, false);
            }
            
            threadInfo->currentLoad.fetch_sub(1);
            
            if (loadBalancingConfig_.enableAdaptive && 
                threadInfo->stats.tasksCompleted % loadBalancingConfig_.adaptiveThreshold == 0) {
                balanceLoad();
            }
        }
    }
}

size_t ThreadPool::selectThread(const Task& task) {
    switch (loadBalancingConfig_.strategy) {
        case LoadBalancingConfig::Strategy::ROUND_ROBIN:
            return roundRobinSelect();
        case LoadBalancingConfig::Strategy::LEAST_LOADED:
            return leastLoadedSelect();
        case LoadBalancingConfig::Strategy::WEIGHTED_ROUND_ROBIN:
            return weightedRoundRobinSelect();
        case LoadBalancingConfig::Strategy::ADAPTIVE:
            return adaptiveSelect();
        default:
            return roundRobinSelect();
    }
}

size_t ThreadPool::roundRobinSelect() {
    size_t current = nextThread_.fetch_add(1);
    return current % threads_.size();
}

size_t ThreadPool::leastLoadedSelect() {
    size_t leastLoaded = 0;
    size_t minLoad = std::numeric_limits<size_t>::max();
    
    for (size_t i = 0; i < threads_.size(); ++i) {
        size_t load = threads_[i]->currentLoad.load();
        if (load < minLoad) {
            minLoad = load;
            leastLoaded = i;
        }
    }
    
    return leastLoaded;
}

size_t ThreadPool::weightedRoundRobinSelect() {
    static size_t currentIndex = 0;
    static size_t currentWeight = 0;
    
    if (loadBalancingConfig_.threadWeights.empty()) {
        return roundRobinSelect();
    }
    
    while (currentWeight == 0) {
        currentIndex = (currentIndex + 1) % threads_.size();
        currentWeight = static_cast<size_t>(loadBalancingConfig_.threadWeights[currentIndex]);
    }
    
    currentWeight--;
    return currentIndex;
}

size_t ThreadPool::adaptiveSelect() {
    size_t bestThread = 0;
    double bestScore = std::numeric_limits<double>::lowest();
    
    for (size_t i = 0; i < threads_.size(); ++i) {
        const auto& stats = threads_[i]->stats;
        size_t currentLoad = threads_[i]->currentLoad.load();
        
        double score = 0.0;
        if (stats.tasksCompleted > 0) {
            score = static_cast<double>(stats.tasksCompleted) / 
                   static_cast<double>(stats.totalProcessingTime.count());
        }
        
        score -= static_cast<double>(currentLoad) * 0.1;
        
        if (score > bestScore) {
            bestScore = score;
            bestThread = i;
        }
    }
    
    return bestThread;
}

void ThreadPool::updateThreadStats(size_t threadId, const std::chrono::microseconds& processingTime, bool success) {
    if (threadId >= threads_.size()) return;
    
    std::lock_guard<std::mutex> lock(statsMutex_);
    auto& stats = threads_[threadId]->stats;
    
    if (success) {
        stats.tasksCompleted++;
    } else {
        stats.tasksFailed++;
    }
    
    stats.totalProcessingTime += processingTime;
    
    if (processingTime > stats.maxProcessingTime) {
        stats.maxProcessingTime = processingTime;
    }
    
    if (processingTime < stats.minProcessingTime) {
        stats.minProcessingTime = processingTime;
    }
    
    if (stats.tasksCompleted > 0) {
        stats.averageProcessingTime = stats.totalProcessingTime / stats.tasksCompleted;
    }
}

void ThreadPool::balanceLoad() {
    for (size_t i = 0; i < threads_.size(); ++i) {
        for (size_t j = i + 1; j < threads_.size(); ++j) {
            if (stealTasks(i, j)) {
                break;
            }
        }
    }
}

bool ThreadPool::stealTasks(size_t thiefId, size_t victimId) {
    if (thiefId >= threads_.size() || victimId >= threads_.size()) return false;
    
    auto& thief = threads_[thiefId];
    auto& victim = threads_[victimId];
    
    std::lock_guard<std::mutex> thiefLock(thief->queueMutex);
    std::lock_guard<std::mutex> victimLock(victim->queueMutex);
    
    if (victim->localQueue.size() <= thief->localQueue.size()) {
        return false;
    }
    
    size_t stealCount = (victim->localQueue.size() - thief->localQueue.size()) / 2;
    
    for (size_t i = 0; i < stealCount && !victim->localQueue.empty(); ++i) {
        thief->localQueue.push(std::move(victim->localQueue.front()));
        victim->localQueue.pop();
    }
    
    return stealCount > 0;
}

void ThreadPool::pinThreadToCore(std::thread& thread, int cpuId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuId, &cpuset);
    
    pthread_t pthread = thread.native_handle();
    pthread_setaffinity_np(pthread, sizeof(cpu_set_t), &cpuset);
}

void ThreadPool::setThreadPriority(std::thread& thread, int priority) {
    pthread_t pthread = thread.native_handle();
    
    sched_param sch_params;
    sch_params.sched_priority = priority;
    pthread_setschedparam(pthread, SCHED_FIFO, &sch_params);
}

} // namespace beatrice 