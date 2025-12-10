#include "../includes/JobScheduler.h"
#include <unordered_set>
#include <algorithm>
#include <iostream>

// JobScheduler Constructor implementation ******************
JobScheduler::JobScheduler(ThreadPool &pool_)
    : pool(pool_), pq(), stop(false), nextId(1)
{
    worker = std::thread([this]()
                         { this->runLoop(); });
}

// JobScheduler destructor implementation  ******************
JobScheduler::~JobScheduler()
{
    // Signal stop and wake the worker thread
    {
        std::lock_guard<std::mutex> lk(mtx);
        stop = true;
    }
    cv.notify_one();
    if (worker.joinable())
        worker.join();
}

// scheduleEvery function implementation *****************
uint64_t JobScheduler::scheduleEvery(Task task, Millis interval, int priority)
{
    uint64_t id = nextId.fetch_add(1);
    ScheduledJob sj{Clock::now() + interval, priority, std::move(task), id, interval};
    {
        std::lock_guard<std::mutex> lk(mtx);
        pq.push(std::move(sj));
    }
    cv.notify_one();
    return id;
}

// cancel (a job) function implementation ****************************
bool JobScheduler::cancel(uint64_t jobId)
{
    std::lock_guard<std::mutex> lk(cancelMtx);
    // Insert to canceled set; runLoop checks this before executing or rescheduling.
    auto res = canceled.insert(jobId);
    return res.second; // true if newly inserted (cancellation requested)
}

// runLoop function (head of the scheduler) function implementation *********************
void JobScheduler::runLoop()
{
    while (true)
    {
        std::vector<ScheduledJob> dueJobs;

        TimePoint waitUntil;
        {
            std::unique_lock<std::mutex> lk(mtx);

            // Wait until there is a job or stop requested
            cv.wait(lk, [&]()
                    { return stop || !pq.empty(); });

            if (stop && pq.empty())
                return;

            // Peek the earliest
            auto next = pq.top();
            auto now = Clock::now();

            if (next.runAt > now)
            {
                // Nothing is due yet: sleep until next.runAt or until notified
                waitUntil = next.runAt;
                // use wait_until to catch new earlier tasks or stop
                cv.wait_until(lk, waitUntil, [&]()
                              { return stop || pq.top().runAt < waitUntil; });
                // loop back to re-evaluate conditions
                continue;
            }

            // Collect all jobs that are due now (runAt <= now)
            now = Clock::now();
            while (!pq.empty() && pq.top().runAt <= now)
            {
                dueJobs.push_back(std::move(const_cast<ScheduledJob &>(pq.top()))); // *******
                pq.pop(); 
            }
        } // unlock mtx here

        if (!dueJobs.empty())
        {
            // Sort dueJobs by priority descending (higher priority first)
            std::sort(dueJobs.begin(), dueJobs.end(),
                      [](const ScheduledJob &a, const ScheduledJob &b)
                      {
                          return a.priority > b.priority;
                      });

            for (auto &job : dueJobs)
            {
                // Check cancellation set
                {
                    std::lock_guard<std::mutex> lk(cancelMtx);
                    if (canceled.find(job.id) != canceled.end())
                    {
                        // If canceled, remove id and skip scheduling
                        canceled.erase(job.id);
                        continue;
                    }
                }

                // Submit to thread pool: move the task into pool
                Task taskToRun; 

                if (job.interval.count() > 0) {
                    taskToRun = job.task; // COPY implementation (std::function copies are cheap enough)
                } else {
                    taskToRun = std::move(job.task); // MOVE only if we never need it again
                }

                pool.SubmitTask([taskToRun]() mutable
                {
                    try {
                        taskToRun();
                    } catch (const std::exception &ex) {
                        // printing any error
                        std::cerr << "Scheduled task exception: " << ex.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Scheduled task unknown exception\n";
                    } });

                // If recurring, reschedule with new runAt
                if (job.interval.count() > 0)
                {
                    ScheduledJob nextJob;
                    nextJob.runAt = Clock::now() + job.interval;
                    nextJob.priority = job.priority;
                    nextJob.task = std::move(job.task); // task might be moved-from if job.task was moved above; avoid double move
                    nextJob.id = job.id;
                    nextJob.interval = job.interval;

                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        pq.push(std::move(nextJob));
                    }
                    cv.notify_one();
                }
            } // end for each due job
        }
    } // while(true)
}
