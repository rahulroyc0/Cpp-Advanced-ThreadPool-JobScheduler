# Cpp-Advanced-ThreadPool-JobScheduler

A high-performance, thread-safe asynchronous task orchestrator written in Modern C++ (C++11/14/17). This system features a custom ThreadPool implementation coupled with a Priority-based Job Scheduler, capable of handling one-off, delayed, and recurring tasks with generic return types.

# Key Features

**Advanced Type Erasure:** Supports scheduling functions with any return type (int, std::string, void, etc.) while maintaining a uniform task queue. Returns std::future<T> to the caller.
**Priority Scheduling:** Tasks are executed based on a Min-Heap (Time) and Max-Heap (Priority) architecture. High-priority tasks run immediately when their time comes.
**Recurring Jobs:** Built-in support for high-frequency recurring tasks (e.g. "Run every 10ms").
**Cancellation Mechanism:** Ability to cancel pending or recurring jobs via unique Job IDs.
**Graceful Shutdown:** Ensures no data loss by allowing running tasks to complete before thread destruction.
**High Throughput:** Stress-tested with 15,000+ concurrent task submissions and simultaneous recurring jobs without race conditions or deadlocks.

# Architecture

The system is divided into two core components:

1.  **ThreadPool (The Muscle):**
     Manages a pool of worker threads.
     Uses a thread-safe queue protected by std::mutex and std::condition_variable.
     Optimized with task submission (move semantics).

2.  **JobScheduler (The Brain):**
     Manages a std::priority_queue of ScheduledJob structs.
     Handles time-based synchronization using cv.wait_until to minimize CPU usage while waiting for future tasks.
     Wraps generic std::packaged_task to bridge the gap between user types and the thread pool.

# Compilation -
g++ -I./includes main.cpp src/ThreadPool.cpp src/JobScheduler.cpp -o main
.\main

# Stress-tested with 15,000+ concurrent task submissions... and it passed
    <img width="1920" height="1080" alt="Screenshot (121)" src="https://github.com/user-attachments/assets/25bd3edf-6693-4af1-9df0-62e9a168d430" />
