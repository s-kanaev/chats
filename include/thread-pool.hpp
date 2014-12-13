#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <cstddef>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

/*
 * thread pool class.
 */

class ThreadPool {
public:
    // typedef for typing purposes
    typedef boost::shared_ptr<boost::asio::io_service> IoServicePtr;
    typedef boost::shared_ptr<boost::thread_group> WorkerThreadGroupPtr;
    typedef boost::shared_ptr<boost::asio::io_service::work> WorkerPtr;

    /*
     * construct a thread pool with _threads_count threads
     */
    ThreadPool(std::size_t _threads_count = 1,
               IoServicePtr _io_service = IoServicePtr(),
               WorkerThreadGroupPtr _thread_group = WorkerThreadGroupPtr()) :
        m_threads_count(_threads_count),
        m_io_service(_io_service),
        m_thread_group(_thread_group),
        m_io_service_guard(WorkerPtr())
    {
        if (!m_threads_count)
            m_threads_count = 1;
        if (!m_io_service.get())
            m_io_service.reset(new boost::asio::io_service);
        if (!m_thread_group.get())
            m_thread_group.reset(new boost::thread_group);
        if (!m_io_service_guard.get())
            m_io_service_guard.reset(new boost::asio::io_service::work(*m_io_service));

        for (std::size_t i = 0; i < m_threads_count; ++i) {
            m_thread_group->create_thread(boost::bind(&boost::asio::io_service::run,
                                                      m_io_service.get()));
        }
    }

    ~ThreadPool()
    {
        // remove the io_service guard worker
        m_io_service_guard.reset();
        // wait untill all threads end
        m_thread_group->join_all();
    }

    /*
     * put a job _job to pool
     */
    void post(boost::function<void()> _job)
    {
        m_io_service->post(_job);
    }

    /*
     * get threads count
     */
    std::size_t ThreadCount(void) const
    {
        return m_threads_count;
    }
protected:
    // threads count
    std::size_t m_threads_count;
    // io_service for thread group
    IoServicePtr m_io_service;
    // thread group
    WorkerThreadGroupPtr m_thread_group;
    // sentinel for io_service not to shutdown when we don't have any active threads
    WorkerPtr m_io_service_guard;

private:
    // we won't let anybody a copy of thread pool
    ThreadPool(ThreadPool const&);
    ThreadPool& operator=(ThreadPool const);
};

#endif // THREADPOOL_HPP
