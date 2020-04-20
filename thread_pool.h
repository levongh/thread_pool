#pragma once

/**
 * @file thread_pool.h
 * @class thread_pool
 * @description class thread_pool
 * @author Levon Ghukasyan
 */

#include <mutex>
#include <thread>
#include <condition_variable>

#include <type_traits>
#include <future>
#include <functional>

#include <queue>
#include <vector>


class thread_pool
{
public:
    /// @brief constructor
    explicit thread_pool(unsigned thread_count = 0);

    /// @brief destrcutor
    ~thread_pool();

public:
    /// @brief function to shedule tasks
    template <class Task, class... Args>
    auto shedule(Task&& function, Args&&... args) -> std::future<typename std::result_of<Task(Args...)>::type>;

private:
    bool m_stop;

    std::condition_variable m_condition;

    std::mutex m_mutex;

    std::queue<std::function<void()> > m_tasks;

    std::vector<std::thread> m_threads;
};

thread_pool::thread_pool(unsigned thread_count)
    : m_stop{false}
{
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }
    for (unsigned i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this] {
            while(true) {
                std::function<void()> tsk;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    while (!m_stop && m_tasks.empty()) {
                        m_condition.wait(lock);
                    }
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    tsk = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                tsk();
            }
        });
    }
}

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

template <class Task, class... Args>
auto thread_pool::shedule(Task&& function, Args&&... args) -> std::future<typename std::result_of<Task(Args...)>::type>
{
    if(m_stop) {
        throw std::runtime_error("shedule on destroyed sheduler");
    }
    using result_type = decltype(function(args...));

    auto task = std::make_shared< std::packaged_task<result_type()> >(
            std::bind(std::forward<Task>(function), std::forward<Args>(args)...)
         );

    std::future<result_type> result(task->get_future());
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if(m_stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        m_tasks.emplace([task](){ (*task)(); });
    }
    m_condition.notify_one();

    return result;
}
