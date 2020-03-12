// https://docs.microsoft.com/ru-ru/cpp/intrinsics/interlockedcompareexchange-intrinsic-functions?view=vs-2019

#include "dolphin.h"

namespace MySpinLock
{
    #pragma intrinsic(_InterlockedCompareExchange, _InterlockedExchange)

    void Lock(PLOCK pl)
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

    void Unlock(PLOCK pl) {
#if !defined(SKIP_LOCKING)
        _InterlockedExchange((long*)pl, LOCK_IS_FREE);
#endif
    }
}
