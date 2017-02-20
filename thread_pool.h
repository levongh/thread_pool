#pragma once

/**
 * @file thread_pool.h
 * @class thread_pool
 * @description class thread_pool
 * @author Levon Ghukasyan
 */

#include <mutex>
#include <thread>
#include <conditional_variable>

#include <type_traits>
#include <functional>

#include <queue>
#include <vector>


template <typename Task>
class thread_pool
{
public:
    /// @brief constructor
    explicit thread_pool(unsigned thread_count = 0);

    /// @brief destrcutor
    ~thread_pool();

public:
    /// @brief function to shedule tasks
    template <class... Args>
    auto shedule(Task&& function, Args&&... args) -> std::future<typename std::result_of<Task(Args...)>::type>;

private:
    bool m_stop;

    std::conditional_variable m_condition;

    std::mutex m_mutex;

    std::queue<typename Task> m_tasks;

    std::vector<std::thread> m_threads;
};

template <typename Task>
thread_pool::thread_pool(unsigned thread_count)
    : m_stop{false}
{
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }
    for (unsigned i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this] {
            while(true) {
                std::unique_lock<std::mutex> lock(m_mutex);
                while (!stop && m_tasks.empty()) {
                    m_condition.wait(lock);
                }
                if (stop && m_tasks.empty()) {
                    return;
                }
                Task tsk = std::move(m_tasks.front());
                m_tasks.pop();
                lock.unlock();
                tsk();
            }
        });
    }
}

template <typename Task>
thread_pool::~thread_pool()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stop = true;
    }

    m_condition.notify_all();
    for (auto& iter : m_threads) {
        iter.join();
    }
}

template <typename Task>
template <class... Args>
auto thread_pool::shedule(Task&& function, Args&&... Args) -> std::future<typename std::result_of<Task(Args...)>::type>
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(stop) {
        throw std::runtime_error("shedule on destroyed sheduler");
    }
    using result_type = typename std::result_of<Task(Args...)>::type;

    std::packaged_task<result_type()> task(std::bind(std::forward<Task>(function), std::forward<Args>(args)...));

    std::future<result_type> result(task.get_future());
    m_tasks.push(std::move(task));
    conditional_variable.notify_one();

    return result;
}
