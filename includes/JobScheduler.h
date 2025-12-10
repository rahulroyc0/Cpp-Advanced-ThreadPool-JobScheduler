#ifndef JOBSCHEDULER_H
#define JOBSCHEDULER_H

#include "ThreadPool.h" //  existing ThreadPool header

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <chrono>
#include <atomic>
#include <vector>
#include <unordered_set>
#include <future>

class JobScheduler
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Millis = std::chrono::milliseconds;
    using Task = std::function<void()>;

    // Construct with a reference to a ThreadPool
    explicit JobScheduler(ThreadPool &pool);

    // Destructor: stops scheduler and joins internal thread
    ~JobScheduler();

    // Schedule a task to run after `delay` milliseconds.
    template <class F, class... Args>
    auto scheduleAfter(Millis delay, int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>;

    // SsheduleAt ..... Schedule a task at a specific time point...it is a template function
    // it can take any function and then wrap it as void function
    template <class F, class... Args>
    auto scheduleAt(TimePoint when, int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>;

    // Schedule a task to run immediately with a priority.... it is a template function
    // it can take any function and then wrap it as void function
    template <class F, class... Args>
    auto scheduleWithPriority(int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>;

    // Schedule a recurring task: first run after `interval`, then every `interval`.....Returns a handle id (uint64_t) that can be used to cancel (optional).
    uint64_t scheduleEvery(Task task, Millis interval, int priority = 0);

    // Optional: cancel a scheduled recurring job or pending job by id .
    // If it was pending it will be removed; if already executing, cancellation will fail (returns false).
    bool cancel(uint64_t jobId);

private:
    // ScheduleJob struct for grouping the task with its id,interval,runTime etc
    struct ScheduledJob
    {
        TimePoint runAt;  // next time to run
        int priority = 0; // higher -> run earlier among same time
        Task task;
        uint64_t id;     // unique id for cancellation/recurring
        Millis interval; // zero if not recurring, otherwise periodic interval
    };
    // Compare struct for priority_queue
    struct Compare
    {
        bool operator()(JobScheduler::ScheduledJob const &a, JobScheduler::ScheduledJob const &b) const
        {
            if (a.runAt != b.runAt)
                return a.runAt > b.runAt;
            return a.priority < b.priority;
        }
    };

    ThreadPool &pool; // reference to user's threadpool
    std::priority_queue<ScheduledJob, std::vector<ScheduledJob>, Compare> pq;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker; // scheduler thread
    std::atomic<bool> stop;
    std::atomic<uint64_t> nextId; // for generating unique job ids

    // map to help cancellation
    std::mutex cancelMtx;
    std::unordered_set<uint64_t> canceled;

    // main loop run by `worker`
    void runLoop();
};

#endif

// ------- scheduleAt function (template) implementation (for general function) ------------------------------------
template <class F, class... Args>
auto JobScheduler::scheduleAt(TimePoint when, int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>
{

    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    Task voidWrapper = [task]()
    {
        (*task)();
    };
    uint64_t id = nextId.fetch_add(1);
    ScheduledJob sj{when, priority, std::move(voidWrapper), id, Millis(0)};
    {
        std::lock_guard<std::mutex> lk(mtx);
        pq.push(std::move(sj));
    }
    cv.notify_all();
    return res;
}

// ------ scheduleAfter function (template) implementation (for general function) ---------------------
template <class F, class... Args>
auto JobScheduler::scheduleAfter(Millis delay, int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>
{
    return scheduleAt(Clock::now() + delay, priority, std::forward<F>(f), std::forward<Args>(args)...);
}

// ------- scheduleWithPriority function (template) implementation (for general function) --------------------
template <class F, class... Args>
auto JobScheduler::scheduleWithPriority(int priority, F &&f, Args &&...args) -> std::future<decltype(f(args...))>
{
    return scheduleAt(Clock::now(), priority, std::forward<F>(f), std::forward<Args>(args)...);
}
