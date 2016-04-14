#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>

#include "signal_lock.hpp"

using namespace std;
using namespace chrono;

static const unsigned long inc_count = 1 << 18;
static const unsigned nthreads = 8;

static const int test_signal = SIGUSR1;
using lock_t = signal_lock<test_signal>;

// the variable that we increment from thread + signal context
static lock_t count_lock;
static unsigned long count = 0;

// how many times did we increment count from signal context?
static thread_local unsigned long sig_calls = 0;
// how many times did we increment count from thread context?
static thread_local unsigned long thread_calls = 0;

static std::mutex totals_lock;
static unsigned long sig_calls_total = 0;
static unsigned long thread_calls_total = 0;

static void handler(int)
{
    sig_calls++;
    lock_guard<lock_t> l_(count_lock);
    count++;
}

static void test_one()
{
    pthread_t tid = pthread_self();

    // thread to raise signals to poke the current thread
    thread signaler([&]() {
            for (size_t i = 0; i < inc_count; ++i)
                pthread_kill(tid, test_signal);
        });

    for (size_t i = 0; i < inc_count; ++i) {
        thread_calls++;
        lock_guard<lock_t> l_(count_lock);
        count++;
    }

    {
        lock_guard<std::mutex> l_(totals_lock);
        sig_calls_total += sig_calls;
        thread_calls_total += thread_calls;
    }
    
    signaler.join();
}

int main()
{
    signal(test_signal, handler);

    thread threads[nthreads];

    for (size_t i = 0; i < nthreads; ++i)
        threads[i] = thread(test_one);

    for (size_t i = 0; i < nthreads; ++i)
        threads[i].join();

    cout << "thread_calls_total is: " << thread_calls_total << endl;
    cout << "sig_calls_total is: " << sig_calls_total << endl;
    
    assert(thread_calls_total = nthreads*inc_count);
    assert(sig_calls_total + thread_calls_total == count);
}
