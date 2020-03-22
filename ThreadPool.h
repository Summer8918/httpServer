#pragma once
#ifndef THREAD_POOL_HPP
#include<iostream>
#include<vector>
#include<queue>
#include<string>
#include <future> //std::future
#include<thread> //this_thread::sleep_for
#include <chrono> //std::chrono::seconds
#include<functional> //std::bind
class ThreadPool{
public:
    //在线程池中创建threads个工作线程
    ThreadPool(size_t threads);
    //向线程池中增加线程
    //可变参数的模板，class/typename...Args是模板形参包，接受0或更多模板实参的模板形参
    //enqueue函数会让一个新的线程被传入进去
    //第一个参数中使用F &&来进行右值引用，而不会发生拷贝行为而再次创建一个执行体
    //其返回类型，设计程获得要执行函数体在执行完毕后返回地结果
    //必须推断出返回类型是什么，才能写出std::future<typename 返回类型>的代码
    //实际上执行函数的返回类型可以被std::result_of推断
    template<typename F,typename... Args>
        auto enqueue(F&& f,Args&&... args)
       -> std::future<typename std::result_of<F(Args...)>::type>;
    //尾置返回类型
    //类模板std::future提供访问异步操作结果的机制
    //通过std::async,std::packaged_task或std::promise创建的）异步操作
    //能提供一个std::future对象给该异步操作的创建者
    //然后异步操作的创建者能用各种方法查询、等待或从std::future提取值

    ~ThreadPool();
private:
    //需要持续追踪线程来保证可以使用join
    std::vector<std::thread> workers;
    //任务队列
    //类模板std::functin是通用多态函数封装器。
    //std::function的实例能存储、复制及调用任何可调用的目标
    //包括函数、lambda表达式、bind表达式或其他函数对象，还有指向成员函数指针和指向数据成员指针
    /* 存储的可调用对象被称为std::function的目标，若std::function不含目标，则称它为空
     *
     * */
    std::queue<std::function<void()>> tasks;

    /*同步相关
     * 互斥锁
     * std::mutex 是能用于保护共享数据免受从多个线程同时访问的同步原语
     * mutex提供排他性非递归所有权语义：
     * 调用方线程从它成功调用lock或try_lock开始，到它调用unlock为止占有mutex
     * 线程占有mutex时，所有其他线程若试图要求mutex的所有权，则将阻塞（对于lock的调用）或收到false的返回值（对于try_lock)
     * 调用方线程在调用lock或try_lock前必须不占有mutex
     * 若 mutex 在仍为任何线程所占有时即被销毁，或在占有 mutex 时线程终止，则行为未定义
     * std::mutex 既不可复制亦不可移动。
     */
    std::mutex queue_mutex;  
    /* 互斥条件变量
     * condition_variable类是同步原语，能用于阻塞一个线程或同时阻塞多个线程，直到另一线程修改共享变量(条件）并通知condition_variable
     * 有意修改变量的线程必须：
     * 1 获得std::mutex（典型地通过std::lock_guard)
     * 2 在保有锁时进行修改
     * 3 在std::condition_variable上执行notify_one或notify_all(不需要为通知保有锁）
     * 即使共享变量是原子的，也必须在互斥下修改它，以正确地发布修改到等待的线程。 
     * 任何有意在std::condtion_variable上等待地线程必须：
     * 1 获得std::unique_lock<std::mutex>,在与用于保护共享变量者相同地互斥上
     * 2  执行 wait 、 wait_for 或 wait_until ，等待操作自动释放互斥，并悬挂线程的执行。
     *3 condition_variable 被通知时，时限消失或虚假唤醒发生，线程被唤醒，且自动重获得互斥。之后线程应检查条件，若唤醒是虚假的，则继续等待。
     * */
    std::condition_variable condition;
    //停止相关
    bool stop;
};

 /*构造函数
 * 构造的线程池指定可并发执行的线程的数量
 * 任务的执行和结束阶段位于临界区，保证不会并发的同时启动多个需要执行的任务
 */

inline ThreadPool::ThreadPool(size_t threads):stop(false){
    //启动threads数量的工作线程worker
    for(size_t i=0;i<threads;++i){
        std::cout<<"threads i="<<i<<std::endl;
        workers.emplace_back(
                //此处的lambda表达式捕获this，即线程池实例
                [this]{
                //循环避免虚假唤醒
                for(;;){
                //定义函数对象的容器，存储任意的返回类型为void，参数表为空的函数
                    std::function<void()> task;
                    //临界区
                    {
                        //创建互斥锁
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
        
                        //阻塞当前线程，直到condition_variable被唤醒 等待lambda表达式返回true
                        this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty();});

                        if(this->stop && this->tasks.empty()){
                            return;
                        }

                        //否则就让任务队列的队首任务作为需要执行的任务出队
                        std::cout<<"start handle task"<<std::endl;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    //执行当前任务
                    task();
                }
                }
        );
    }
}

/*
 * 线程池的销毁对应构造时究竟创建了什么实例
 * 在销毁线程池之前，我们不知道当前线程池中的工作线程是否执行完成，所以必须先创建一个临界区
 * 将线程池状态标记为停止，从而禁止新的线程的加入，最后等待所有执行线程的运行结束，完成销毁
 * */

inline ThreadPool::~ThreadPool(){
    //临界区
    {  
        //创建互斥锁
        std::unique_lock<std::mutex> lock(queue_mutex);

        //设置线程池状态
        stop=true;
    }

    //通知所有等待线程
    condition.notify_all();

    //使所有异步线程转为同步执行
    for(std::thread &worker:workers)
        worker.join();
}

/*
 * 向线程池中添加新任务
 * 支持多个入队任务参数需要使用变长模板参数
 * 为了调度执行的任务，需要包装执行的任务，这就意味着需要对任务函数的类型进行包装、构造
 * 临界区可以在一个作用域里边被创建，最佳实践是使用RAII的形式
 * RAII “资源获取即初始化方式”(RAII，Resource Acquisition Is Initialization)
 * */
template<class F,class... Args>
auto ThreadPool::enqueue(F &&f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    //推导任务返回类型
    using return_type = typename std::result_of<F(Args...)>::type;

    //获得当前任务
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f),std::forward<Args>(args)...)
            );

    //获得std::future对象以供实施线程同步
    std::future<return_type> res = task->get_future();
    
    //临界区
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        //禁止在线程池停止后加入新的线程
        if(stop){
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        //将线程添加到执行任务队列中
        tasks.emplace([task]{ (*task)();});
    }

    //通知一个正在等待的线程
    condition.notify_one();
    return res;
}

#endif // THREAD_POOL_HPP