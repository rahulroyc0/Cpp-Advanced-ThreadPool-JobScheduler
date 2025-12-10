#include "../includes/ThreadPool.h"
#include <iostream>
#include<thread>
#include<vector>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<future>
using namespace std;

// ThreadPool constructor implementation **********************************
	ThreadPool::ThreadPool(int numThreads) : m_threads(numThreads), stop(false) {
		for (int i = 0; i < m_threads; i++) {
			threads.emplace_back([this] {
				function <void()> task;
				while (1) {
					unique_lock<mutex> lock(mtx);
					cv.wait(lock, [this] {
						return !tasks.empty() || stop;
						});

					if (stop && tasks.empty()) {
						return;
					}

					task = move(tasks.front());
					tasks.pop();
					// cout << "size of queue : " << tasks.size() << endl;
					lock.unlock();
					task();
				}
				});
		}
	}
	
	// ThreadPool destructor implementation *************************
	ThreadPool::~ThreadPool() {
		unique_lock<mutex> lock(mtx);
		stop = true;
		lock.unlock();
		cv.notify_all();
		for (auto& th : threads) {
			th.join();
		}
	}

	// ThreadPool SubmitTask function implementation (this function is mainly for jobScheduler) ************
	void ThreadPool::SubmitTask(std::function<void()> task){
        {
			std::unique_lock<std::mutex> lock(this->mtx);
			tasks.emplace(std::move(task));
		}
		this->cv.notify_all();
	}