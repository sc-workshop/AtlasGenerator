#include "ParallelFor.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wk::AtlasGenerator::packing {
    namespace {
        using Body = std::function<void(size_t)>;

        size_t configured_thread_count() {
            static const size_t count = [] {
                const char* value = std::getenv("ATLAS_GENERATOR_THREADS");
                if (value != nullptr) {
                    try {
                        const int parsed = std::stoi(value);
                        if (parsed > 0)
                            return static_cast<size_t>(parsed);
                    } catch (...) {
                    }
                }
                const unsigned int detected = std::thread::hardware_concurrency();
                return detected > 0 ? static_cast<size_t>(detected) : size_t {1};
            }();
            return count;
        }

        class ThreadPool {
        public:
            explicit ThreadPool(size_t thread_count) {
                const size_t count = std::max<size_t>(1, thread_count);
                m_workers.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    m_workers.emplace_back([this] { worker(); });
                }
            }

            ~ThreadPool() {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stop = true;
                }
                m_work.notify_all();
                for (std::thread& worker : m_workers) {
                    if (worker.joinable())
                        worker.join();
                }
            }

            void run(size_t count, const Body& body) {
                std::lock_guard<std::mutex> run_lock(m_run_mutex);

                auto work = std::make_shared<Work>(count, &body);
                unsigned long long epoch = 0;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_done = false;
                    m_current = std::move(work);
                    epoch = ++m_epoch;
                }
                m_work.notify_all();

                std::unique_lock<std::mutex> lock(m_mutex);
                m_done_cv.wait(lock, [this, epoch] { return m_stop || (m_done && m_epoch == epoch); });
            }

        private:
            struct Work {
                Work(size_t item_count, const Body* item_body) :
                    count(item_count),
                    body(item_body),
                    remaining(item_count) {}

                size_t count;
                const Body* body;
                std::atomic<size_t> next {0};
                std::atomic<size_t> remaining {0};
            };

            void worker() {
                unsigned long long observed_epoch = 0;
                std::shared_ptr<Work> work;
                for (;;) {
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_work.wait(lock, [this, observed_epoch] { return m_stop || m_epoch != observed_epoch; });
                        if (m_stop)
                            return;
                        observed_epoch = m_epoch;
                        work = m_current;
                    }

                    for (;;) {
                        const size_t index = work->next.fetch_add(1, std::memory_order_relaxed);
                        if (index >= work->count)
                            break;

                        (*work->body)(index);

                        if (work->remaining.fetch_sub(1, std::memory_order_relaxed) == 1) {
                            {
                                std::lock_guard<std::mutex> lock(m_mutex);
                                m_done = true;
                            }
                            m_done_cv.notify_all();
                            break;
                        }
                    }
                    work.reset();
                }
            }

            std::vector<std::thread> m_workers;
            std::mutex m_run_mutex;
            std::mutex m_mutex;
            std::condition_variable m_work;
            std::condition_variable m_done_cv;
            std::shared_ptr<Work> m_current;
            unsigned long long m_epoch = 0;
            bool m_done = false;
            bool m_stop = false;
        };

        ThreadPool& thread_pool() {
            static ThreadPool pool(configured_thread_count());
            return pool;
        }
    }

    void parallel_for_indexed(size_t count, const std::function<void(size_t)>& body) {
        if (count == 0)
            return;
        if (count == 1) {
            body(0);
            return;
        }
        thread_pool().run(count, body);
    }
}
