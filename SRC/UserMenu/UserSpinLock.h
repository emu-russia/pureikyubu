// https://docs.microsoft.com/ru-ru/cpp/intrinsics/interlockedcompareexchange-intrinsic-functions?view=vs-2019

#pragma once

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
namespace MySpinLock
{
    typedef volatile long LOCK, * PLOCK;

    enum { LOCK_IS_FREE = 0, LOCK_IS_TAKEN = 1 };

    void Lock(PLOCK pl);
    void Unlock(PLOCK pl);
}
