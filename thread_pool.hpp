#pragma once
#include <list>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <condition_variable>
#include <stdexcept>
#include <memory>

namespace ccat {

class thread_pool final {
#if defined(__cplusplus) && __cplusplus >= 201703L || defined(_MSVC_LANG ) && _MSVC_LANG >= 201703L
    template<typename F, typename... Args>
    using result_of = std::invoke_result_t<F, Args...>;
#else
    template<typename F, typename... Args>
    using result_of = typename std::result_of<F(Args...)>::type;
#endif
    using task_wrapper = std::function<void()>;
public:
    explicit thread_pool(size_t worker_count_ = std::thread::hardware_concurrency()) : worker_cnt_(worker_count_), busy_worker_cnt_(0) {
        for (size_t i{}; i < worker_cnt_; ++i) {
            workers_.emplace_back(std::bind(&thread_pool::fetch_and_finish_task_, this));
        }
    }
    thread_pool(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    auto operator= (const thread_pool&) ->thread_pool& = delete;
    auto operator= (thread_pool&&) ->thread_pool& = delete;

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) ->std::future<result_of<F, Args...>> {
        if (stop_) throw std::runtime_error{"submit task after thread pool shutdown"};
        using rt = result_of<F, Args...>;
        auto pptask = std::make_shared<std::packaged_task<rt()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto future_ = pptask->get_future();
        std::lock_guard<std::mutex> lg{mtx_};
        task_queue_.emplace(
            [pptask]() mutable {(*pptask)();}
        );
        cv_.notify_one();
        return future_;
    }
    auto shutdown(bool wait = true) ->void {
        if (wait) {
            wait_until_all_tasks_finished();
        }
        stop_ = true;
        cv_.notify_all();
        for (auto&& worker : workers_) {
            worker.join();
        }
    }
    auto wait_until_all_tasks_finished() ->void {
        if (stop_) return;
        std::unique_lock<std::mutex> ulk{mtx_};
        cv_1_.wait(ulk, [this]{
            return task_queue_.empty();
        });
    }
    auto max_worker_count() noexcept ->size_t {
        return worker_cnt_;
    }
    auto current_busy_worker_count()  noexcept ->size_t {
        return busy_worker_cnt_;
    }
    auto get_all_worker_ids() ->std::vector<std::thread::id> {
        std::vector<std::thread::id> res;
        for (auto&& worker : workers_) {
            res.push_back(worker.get_id());
        }
        return res;
    }
    ~thread_pool() {
        if (!stop_) shutdown();
    }
private:
    auto fetch_and_finish_task_() ->void {
        for (;;) {
            std::unique_lock<std::mutex> ulk{mtx_};
            cv_.wait(ulk, [this]{return !task_queue_.empty() || stop_;});
            if (stop_) return;
            ++busy_worker_cnt_;
            auto task = std::move(task_queue_.front());
            task_queue_.pop();
            ulk.unlock();
            task();
            cv_1_.notify_one();
            --busy_worker_cnt_;
        }
    }
private:
    size_t worker_cnt_;
    std::atomic<size_t> busy_worker_cnt_;
    std::atomic<bool> stop_;
    std::condition_variable cv_, cv_1_;
    std::mutex mtx_;
    std::list<std::thread> workers_;
    std::queue<task_wrapper> task_queue_;
};

}
