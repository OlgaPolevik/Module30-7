//
//  main.cpp
//  Module30-7
//
//  Created by Ольга Полевик on 20.01.2022.
//

#include <iostream>
#include <functional>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <vector>

typedef std::function<void(void)> Task;

class ThreadPool
{
public:
    ThreadPool()
        : work( true )
    {
        for ( uint i = 0; i < std::thread::hardware_concurrency(); ++i )
        {
            pool.emplace_back(
                              std::make_unique<Worker>(
                                                       [this, i]() { thread( i ); }
                                                       )
                              );
        }
    }
    
    ~ThreadPool()
    {
        {
            std::shared_lock<std::shared_mutex> lock( signal_mutex );
            work = false;
        }
        signal.notify_all();
        
        for ( auto& worker : pool )
            worker->join();
        
        pool.clear();
    }
    
    void push_front( Task task )
    {
        for ( auto& worker : pool )
        {
            if ( worker->thread.get_id() == std::this_thread::get_id() )
            {
                worker->push_front( std::move( task ) );
                signal.notify_all();
                return;
            }
        }
        
        {
            std::unique_lock<std::mutex> lock( mutex );
            queue.push_front( std::move( task ) );
        }
        
        signal.notify_all();
    }
    
private:
    Task pop_back()
    {
        Task result;
        
        std::unique_lock<std::mutex> lock( mutex );
        
        if ( !queue.empty() )
        {
            result = queue.back();
            queue.pop_back();
        }
            
        return result;
    }
        
    void thread( uint index )
    {
        for ( ; work; )
        {
            if ( Task task = pool[index]->pop_front() )
            {
                task();
                continue;
            }
            
            if ( Task task = pop_back() )
            {
                task();
                continue;
            }
            
            for ( uint i = 0; i < pool.size() - 1; ++i  )
            {
                uint other_worker_index = ( index + 1 + i ) % pool.size();

                if ( Task task = pool[other_worker_index]->pop_back() )
                {
                    task();
                    continue;
                }
            }
            
            std::shared_lock<std::shared_mutex> lock( signal_mutex );
            signal.wait( lock );
        }
    }
    
    struct Worker
    {
        template<typename F>
        Worker( F fn )
            : thread( std::forward<F>( fn ) )
        {
        }
        
        ~Worker()
        {
            join();
        }
        
        void join()
        {
            if ( thread.joinable() )
                thread.join();
        }
        
        void push_front( Task task )
        {
            std::unique_lock<std::mutex> lock( mutex );
            queue.push_front( std::move( task ) );
        }
        
        Task pop_front()
        {
            Task result;
            
            std::unique_lock<std::mutex> lock( mutex );
            
            if ( !queue.empty() )
            {
                result = queue.front();
                queue.pop_front();
            }
                
            return result;
        }
        
        Task pop_back()
        {
            Task result;
            
            std::unique_lock<std::mutex> lock( mutex );
            
            if ( !queue.empty() )
            {
                result = queue.back();
                queue.pop_back();
            }
                
            return result;
        }

        std::thread thread;
        std::deque<Task> queue;
        std::mutex mutex;
    };
    
    std::deque<Task> queue;
    std::mutex mutex;
    
    std::vector<std::unique_ptr<Worker>> pool;
    
    std::atomic<bool> work;
    std::shared_mutex signal_mutex;
    std::condition_variable_any signal;
};

int main(int argc, const char * argv[]) {
    ThreadPool pool;
    
    for ( int i = 0; i < 10000; ++i  )
    {
        pool.push_front( [&pool]()
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( std::rand() % 100 ) );
            
            if ( std::rand() % 2 == 0 )
            {
                pool.push_front( [](){
                    std::this_thread::sleep_for( std::chrono::milliseconds( std::rand() % 100 ) );
                } );
            }
        });
    }
    
    std::this_thread::sleep_for( std::chrono::milliseconds( 10000 ) );
    
    return 0;
}
