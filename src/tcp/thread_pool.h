#pragma once
#pragma once
#include <queue>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include "filetime.h"

namespace mplc {
    typedef std::function<void()> paralel_task_type;

    class Worker {
    public:
        struct Task {
            paralel_task_type task;
            FileTime period;
            FileTime timer;
            std::atomic<int> running;
            int id;
            typedef std::shared_ptr<Task> ptr;
            typedef std::weak_ptr<Task> weak_ptr;
            static ptr make(int id, paralel_task_type task, int64_t period = 0);
            void run();
        };
        typedef std::shared_ptr<Worker> ptr;
        Worker();
        void push(Task::weak_ptr task) {
            std::lock_guard<std::mutex> lock(mtx);
            f_queue.push(task);
            cv.notify_one();
        }
        size_t getTaskCount() {
            std::lock_guard<std::mutex> lock(mtx);
            return f_queue.size();
        }
        bool isEmpty() {
            std::lock_guard<std::mutex> lock(mtx);
            return f_queue.empty();
        }
        virtual ~Worker() {
            enabled = false;
            cv.notify_one();
            m_th.join();
        }
        // private:
        bool enabled;
        bool run;
        std::queue<Task::weak_ptr> f_queue;
        std::condition_variable cv;
        std::mutex mtx;
        std::thread m_th;
        void thread_fn();
    };
    class ParalelTasksPool {
        bool enabled;
        FileTime last_time;
        std::vector<Worker::ptr> _workers;
        std::vector<Worker::Task::ptr> periodic_tasks;
        std::vector<int64_t> wait_time;
        std::mutex mtx;
        size_t thread_count;
        std::thread* m_th;
        void thread_fn();
        Worker::ptr getWorker();
        void RunStop();
        void WaitStop();
        void Start();

    public:
        ParalelTasksPool(size_t threads = 1);
        // void addTask(paralel_task_type fn) { getWorker()->push(fn); }
        int addPeriodTask(int64_t ft_interval, paralel_task_type fn);
        void removeTask(int task_id);
        virtual ~ParalelTasksPool();
        static ParalelTasksPool& instance();
        // void Stop();
    };
    class AsyncTask {
        int tast_id;

    public:
        AsyncTask(int64_t ft_period, paralel_task_type fn) {
            tast_id = ParalelTasksPool::instance().addPeriodTask(ft_period, std::move(fn));
        }
        virtual ~AsyncTask() { ParalelTasksPool::instance().removeTask(tast_id); }
    };

    class AsyncLogger : AsyncTask {
    public:
        virtual void log() { return; }
        AsyncLogger(int64_t ft_period);
        virtual ~AsyncLogger() {}
    };

}  // namespace mplc
