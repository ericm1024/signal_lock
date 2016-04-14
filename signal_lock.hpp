#include <cassert>
#include <mutex>
#include <signal.h>
#include <system_error>

template <int excl_signal, typename lock_type = std::mutex>
class signal_lock {
    lock_type lock_;

    sigset_t saved_ss_;

    // mask of signals we can catch (i.e. not SIGKILL or SIGSTOP. We need this
    // mask so we can static_assert that the template argument is valid in the
    // constructor.
    //
    // NB: This was coppied from the first table in `man signal.h` on a Linux
    // machins, so these are Linux signals only. We need to do horrible
    // preprocessor things to make this portable.
    static constexpr int CATCHABLE_SIG_MASK =
        SIGABRT | SIGALRM | SIGBUS | SIGCHLD | SIGCONT | SIGFPE | SIGHUP |
        SIGILL | SIGINT | SIGPIPE | SIGQUIT | SIGSEGV | SIGSTOP | SIGTERM |
        SIGTSTP | SIGTTIN | SIGTTOU | SIGUSR1 | SIGUSR2 | SIGPOLL | SIGPROF |
        SIGSYS | SIGTRAP | SIGURG | SIGVTALRM | SIGXCPU | SIGXFSZ;
    
public:
    constexpr signal_lock()
    {
        static_assert((excl_signal & ~CATCHABLE_SIG_MASK) == 0,
                      "invalid signal template argument");
    }

    signal_lock(const signal_lock&) = delete;
    signal_lock& operator=(const signal_lock&) = delete;
    ~signal_lock() = default;
    
    void lock()
    {
        sigset_t ss, saved;
        int err;

        err = sigemptyset(&ss);
        assert(!err);
        err = sigaddset(&ss, excl_signal);
        assert(!err);

        err = pthread_sigmask(SIG_BLOCK, &ss, &saved);
        // only possible errors are programming errors
        assert(!err);

        try {
            lock_.lock();
        } catch (const std::system_error& ex) {
            err = pthread_sigmask(SIG_UNBLOCK, &saved, NULL);
            assert(!err);
            throw;
        }
        saved_ss_ = saved;
    }

    void unlock()
    {
        int err;
        sigset_t saved = saved_ss_;

        lock_.unlock();

        // XXX: we only want to modify the signals in the excl_signal mask
        err = pthread_sigmask(SIG_SETMASK, &saved, NULL);
        assert(!err);
    }
};
