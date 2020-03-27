// https://docs.microsoft.com/ru-ru/cpp/intrinsics/interlockedcompareexchange-intrinsic-functions?view=vs-2019

#pragma once

#pragma intrinsic(_InterlockedCompareExchange, _InterlockedExchange)

// if defined, will not do any locking on shared data
//#define SKIP_LOCKING

// A common way of locking using _InterlockedCompareExchange.
// Refer to other sources for a discussion of the many issues
// involved. For example, this particular locking scheme performs well
// when lock contention is low, as the while loop overhead is small and
// locks are acquired very quickly, but degrades as many callers want
// the lock and most threads are doing a lot of interlocked spinning.
// There are also no guarantees that a caller will ever acquire the
// lock.
class MySpinLock
{
public:
    typedef volatile long LOCK;

    enum { LOCK_IS_FREE = 0, LOCK_IS_TAKEN = 1 };

    static void Lock(LOCK* pl)
    {
#if !defined(SKIP_LOCKING)
        // If *pl == LOCK_IS_FREE, it is set to LOCK_IS_TAKEN
        // atomically, so only 1 caller gets the lock.
        // If *pl == LOCK_IS_TAKEN,
        // the result is LOCK_IS_TAKEN, and the while loop keeps spinning.
        while (_InterlockedCompareExchange((long*)pl,
            LOCK_IS_TAKEN, // exchange
            LOCK_IS_FREE)  // comparand
            == LOCK_IS_TAKEN)
        {
            // spin!
        }
        // This will also work.
        //while (_InterlockedExchange(pl, LOCK_IS_TAKEN) ==
        //                             LOCK_IS_TAKEN)
        //{
        //    // spin!
        //}

        // At this point, the lock is acquired.
#endif
    }

    static void Unlock(LOCK* pl) {
#if !defined(SKIP_LOCKING)
        _InterlockedExchange((long*)pl, LOCK_IS_FREE);
#endif
    }
};
