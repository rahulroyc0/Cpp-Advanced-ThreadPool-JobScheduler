#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include "includes/ThreadPool.h"
#include "includes/JobScheduler.h"

using namespace std;

// Atomic counter to verify data safety 
atomic<int> completedTasks(0);

// A Cpu less intensive task (just counting)
void fastTask() {
    completedTasks.fetch_add(1);
}

// A CPU-heavy task (simulating work)
void heavyTask() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    completedTasks.fetch_add(1);
}

int main() {
    cout << "=== STARTING STRESS TEST ===" << endl;

    // 1. Initialize Pool with 4 threads
    ThreadPool pool(4);
    JobScheduler scheduler(pool);

    auto start = chrono::high_resolution_clock::now();

    // 2. FLOOD TEST: Submit 10,000 tasks from the main thread
    cout << "[1] Flooding 10,000 tasks..." << endl;
    for (int i = 0; i < 10000; i++) {
        int priority = i % 10; 
        // cout<<"valForPriorityTask : "<<scheduler.scheduleWithPriority(priority, fastTask).get()<<endl;
        scheduler.scheduleWithPriority(priority, fastTask);
    }

    // 3. RECURRING TEST: Add intense recurring jobs
    cout << "[2] Adding high-frequency recurring jobs..." << endl;
    vector<uint64_t> recurringIds;
    for(int i=0; i<5; i++) {
        // Run every 10ms 
        uint64_t id = scheduler.scheduleEvery(heavyTask, chrono::milliseconds(10), 100);
        recurringIds.push_back(id);
    }

    // 4. CONCURRENCY TEST: Have another thread spam tasks simultaneously
    cout << "[3] Launching external thread spammers..." << endl;
    thread spammer([&scheduler]() {
        for (int i = 0; i < 5000; i++) {
            //cout<<"valForAfterScheduleTask : "<<scheduler.scheduleAfter(chrono::milliseconds(50), 1, fastTask).get()<<endl;
            scheduler.scheduleAfter(chrono::milliseconds(50), 1, fastTask);
        }
    });

    // 5. waiting for 5 seonds 
    cout << "Processing... (Please wait 5s)" << endl;
    this_thread::sleep_for(chrono::seconds(5));

    // 6. Cleanup
    spammer.join();
    for(auto id : recurringIds) {
        scheduler.cancel(id);
    }

    // graceful shutdown time
    this_thread::sleep_for(chrono::seconds(2));

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;

    cout << "=== TEST RESULTS ===" << endl;
    cout << "Time Elapsed: " << diff.count() << " seconds" << endl;
    cout << "Total Tasks Completed: " << completedTasks.load() << endl;

    // Validation
    // I expect 10,000 (flood) + 5,000 (spammer) + Recurring ones (~5 * 500 = 2500)
    if (completedTasks.load() > 15000) {
        cout << "PASS: System handled high load!" << endl;
    } else {
        cout << "WARNING: Throughput lower than expected." << endl;
    }

    return 0;
}