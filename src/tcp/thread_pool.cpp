
#include "thread_pool.h"
#include "platform_conf.h"

namespace mplc {
    using std::lock_guard;
    using std::mutex;
    using std::this_thread::sleep_for;
    using namespace std::chrono;

    Worker::Task::ptr Worker::Task::make(int id, paralel_task_type task, int64_t period) {
        ptr p_task = std::make_shared<Task>();
        p_task->task.swap(task);
        p_task->timer = p_task->period = period;
        p_task->id = id;
        p_task->running = 0;
        return p_task;
    }
    void Worker::Task::run() {
        if(!running) {
            running = 1;
            task();
            running = 0;
        }
    }

    void Worker::thread_fn() {
        while(enabled) {
            std::unique_lock<mutex> guard(mtx);
            while(enabled && f_queue.empty()) cv.wait(guard);
            while(!f_queue.empty()) {
                Task::ptr task = f_queue.front().lock();
                if(task) {
                    mtx.unlock();
                    task->run();
                    mtx.lock();
                }
                f_queue.pop();
            }
        }
    }

    Worker::Worker(): enabled(true), run(true), m_th(&Worker::thread_fn, this) {}

    Worker::ptr ParalelTasksPool::getWorker() {
        int min_tasks = -1;
        size_t worker_id = 0;
        for(size_t i = 0; i < _workers.size(); ++i) {
            Worker::ptr& worker = _workers[i];
            const int task_count = worker->getTaskCount();
            if(worker->isEmpty()) return worker;
            if(min_tasks == -1 || min_tasks > task_count) {
                min_tasks = task_count;
                worker_id = i;
            }
        }
        return _workers[worker_id];
    }

    void ParalelTasksPool::thread_fn() {
        while(enabled) {
            mtx.lock();
            const FileTime diff = FileTime::now() - last_time;
            for(size_t i = 0; i < periodic_tasks.size(); ++i) {
                Worker::Task::ptr& task = periodic_tasks[i];
                if(!task) continue;
                task->timer -= diff;
                if(task->timer <= 0) {
                    getWorker()->push(task);
                    task->timer = task->period;
                }
            }
            last_time = FileTime::now();
            mtx.unlock();
            sleep_for(milliseconds(10));
        }
    }

    void ParalelTasksPool::Start() {
        last_time = FileTime::now();
        if(thread_count == 0) thread_count = 1;
        for(size_t i = 0; i < thread_count; i++) { _workers.push_back(std::make_shared<Worker>()); }
        enabled = true;
        m_th = new std::thread(&ParalelTasksPool::thread_fn, this);
    }

    void ParalelTasksPool::RunStop() {
        enabled = false;
        for(size_t i = 0; i < _workers.size(); ++i) { _workers[i]->enabled = false; }
        periodic_tasks.clear();
    }
    void ParalelTasksPool::WaitStop() {
        if(m_th) m_th->join();
        delete m_th;
        m_th = nullptr;
        _workers.clear();
    }
    ParalelTasksPool::ParalelTasksPool(size_t threads): enabled(false), thread_count(threads), m_th(nullptr) {
        Start();
    }

    int ParalelTasksPool::addPeriodTask(int64_t ft_interval, paralel_task_type fn) {
        lock_guard<mutex> lock(mtx);
        int free_id = -1;
        for(size_t i = 0; i < periodic_tasks.size(); ++i) {
            if(!periodic_tasks[i]) {
                free_id = i;
                break;
            }
        }
        if(free_id == -1) {
            free_id = periodic_tasks.size();
            periodic_tasks.push_back(Worker::Task::ptr());
        }
        periodic_tasks[free_id] = Worker::Task::make(free_id, std::move(fn), ft_interval);
        return free_id;
    }

    void ParalelTasksPool::removeTask(int task_id) {
        lock_guard<mutex> lock(mtx);
        if(task_id < 0 || task_id >= periodic_tasks.size()) return;
        periodic_tasks[task_id].reset();
    }

    ParalelTasksPool::~ParalelTasksPool() {}

    ParalelTasksPool& ParalelTasksPool::instance() {
        static ParalelTasksPool pool(3);
        return pool;
    }
    /*void ParalelTasksPool::Stop() {

    }*/

    AsyncLogger::AsyncLogger(int64_t ft_period): AsyncTask(ft_period, std::bind(&AsyncLogger::log, this)) {}
}  // namespace mplc
