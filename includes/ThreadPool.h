#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

// ThreadPool Class 
class ThreadPool {
public:
   // TheadPool constructor declaration ************
    explicit ThreadPool(int numThreads);
    // ThreadPool destructor declaration **************
    ~ThreadPool();

    // ExecuteTask function ** THIS IS FOR GENERAL THREADPOOL USE** (it takes function and arguement and push into task queue) *****
    // !IMPORTANT -- if someone calls ThreadPool independently(without jobScheduler) then have to call this 
    template <class F, class... Args>
    auto ExecuteTask(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

    // !IMPORTANT-- this is generally for JobScheduler 
    void SubmitTask(std::function<void()> task);

private:
    int m_threads;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;

    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

// ---------------- TEMPLATE FUNCTION IMPLEMENTATION -----------------

template <class F, class... Args>
auto ThreadPool::ExecuteTask(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {

    using return_type = decltype(f(args...));

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(this->mtx);
        this->tasks.emplace([task]() { (*task)(); });
    }

    this->cv.notify_all();
    return res;
}

#endif // THREADPOOL_H
