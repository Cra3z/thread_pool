# thread_pool
Implement for thread pool(minimum required c++ standard: C++11)
# `ccat::thread_pool`的成员函数
* `ccat::thread_pool`是不可复制不可移动对象, 其复制构造函数, 移动构造函数, 复制赋值函数, 移动赋值函数均被弃置
* `thread_pool(size_t n)`: 设置线程池大小为`n`, 参数`n`可选, 其默认值为`std::thread::hardware_concurrency()`
* `wait_until_all_tasks_finished()`: 等待所有任务结束
* `template<typename F, typename... Args> submit(F&& f, Args&&... args)`: 提交一个任务, 并返回一个`future`对象
* `shutdown(bool wait)`: 关闭线程池, 若参数`wait`为`true`则会先调用`wait_until_all_tasks_finished()`等待所有任务完成
* `max_worker_count()`: 线程池中的线程数
* `current_busy_worker_count()`: 当前正在工作的线程数
* `get_all_worker_ids()`: 将线程池中所有线程的id以std::vector<std::thread::id>的形式返回
* `~thread_pool()`: 若当前线程池未关闭, 则自动调用`.wait_until_all_tasks_finished`
# example
```c++
#include <iostream>
#include "thread_pool.hpp"

auto main() -> int {
    using ccat::thread_pool;
    using namespace std::chrono_literals;

    thread_pool tp{3};
    std::future<int> f1 = tp.submit([] {
        std::this_thread::sleep_for(1s);
        return 1;
    });
    std::future<int> f2 = tp.submit([] {
        std::this_thread::sleep_for(500ms);
        return 2;
    });
    tp.wait_until_all_tasks_finished();
    std::cout << f1.get() << ", " << f2.get() << '\n';
    return 0;
}
```
