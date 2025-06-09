#pragma once

#include "Log.h"
#include "PAL.h"

#include <functional>


// occurs to me I could combine this with ObjectPool logic.
// I only ever get requests for size 1.
// I know where to return it because I have a big contiguous
// block, and (ptr-buf)/sizeof(T) is its index.


// For when you need to allocate under an ISR.
// Garbage implementation, assumes small data sets.
template <typename T, uint8_t POOL_SIZE = 50>
struct IsrPoolHeapAllocator
{
    static const uint8_t DEFAULT_POOL_SIZE = 50;
    
    using value_type = T;

    template <class T2> struct rebind { typedef IsrPoolHeapAllocator<T2, POOL_SIZE> other; };

    IsrPoolHeapAllocator(){}
    template <typename U> IsrPoolHeapAllocator(U&) noexcept {}
    template <typename U> IsrPoolHeapAllocator(U&&) noexcept {}

    [[nodiscard]]
    T *allocate(size_t n) noexcept
    {
        T *retVal = nullptr;

        for (size_t i = 0; i < POOL_SIZE; ++i)
        {
            MemUsage &m = memUsageList_[i];

            if (m.addr_ == nullptr)
            {
                if (n == 1)
                {
                    // winner
                    m.addr_ = (T *)&memList_[i];
                    m.n_    = n;
                    retVal  = m.addr_;
                    break;
                }
                else // (n > 1)
                {
                    // check if you can use these n blocks
                    if (i + (n - 1) < POOL_SIZE)
                    {
                        // well, it fits in the container, that's a start
                        bool clear = true;
                        size_t limit = i + (n-1);
                        uint8_t nSeen;
                        for (size_t j = i + 1; j <= limit; ++j)
                        {
                            if (memUsageList_[j].addr_ != nullptr)
                            {
                                clear = false;
                                nSeen = memUsageList_[j].n_;

                                break;
                            }
                        }

                        if (clear)
                        {
                            // winner, i is unchanged, so still ok to use
                            m.addr_ = (T *)&memList_[i];
                            m.n_   = n;
                            retVal = m.addr_;
                            break;
                        }
                        else
                        {
                            // jump beyond this allocation on next loop
                            i += nSeen - 1;
                        }
                    }
                    else
                    {
                        // fail
                        break;
                    }
                }
            }
            else
            {
                i += m.n_ - 1;
            }
        }

        if (retVal == nullptr)
        {
            LogModeSync();
            Log("ERR: IsrPoolHeapAllocator Alloc(", n, ", ", POOL_SIZE, ")");
            Validate();
        }

        return retVal;
    }

    void deallocate(T *p, size_t n) noexcept
    {
        if (p == nullptr) { return; }

        bool found = false;
        for (int i = 0; i < POOL_SIZE; ++i)
        {
            MemUsage &m = memUsageList_[i];

            if (m.addr_ == p)
            {
                m.addr_ = nullptr;
                found = true;
                break;
            }
        }

        if (!found)
        {
            LogModeSync();
            Log("ERR: IsrPoolHeapAllocator Dealloc(", (uint32_t)p, ", ", n, ", ", POOL_SIZE, ")");
            Validate();
        }
    }

public:

    void Validate(const char *msg = nullptr)
    {
        if (msg)
        {
            Log(msg);
        }

        auto pad = [](size_t i){
            const char *pad = "";
            if (i < 10)
            {
                pad = " ";
            }

            return pad;
        };

        auto maybeNl = [](size_t i){
            if (i % 10 == 9) { LogNL(); }
        };

        for (size_t i = 0; i < POOL_SIZE; ++i)
        {
            if (memUsageList_[i].addr_ == nullptr)
            {
                // Free slot
                LogNNL("[", pad(i), i, ":F]");
                maybeNl(i);
            }
            else
            {
                uint8_t n = memUsageList_[i].n_;

                // used slot - n of them including this one
                LogNNL("[", pad(i), i, ":U:", n, "]");
                maybeNl(i);

                // validate that the following n-1 are not allocated explicity
                // as they are supposed to be blank as they are allocated implicitly
                // by the first entry and its 'n' value
                if (i + (n-1) >= POOL_SIZE)
                {
                    // implies the 'n' value itself is corrupted
                    LogNL();
                    Log("ERR: i+n >= POOL_SIZE: ", i, " ", n);
                    LogNL();
                }
                else
                {
                    // check each to confirm empty
                    size_t limit = i + (n-1);
                    for (size_t j = i + 1; j <= limit; ++j)
                    {
                        i = j;

                        if (memUsageList_[i].addr_ == nullptr)
                        {
                            // good condition
                            LogNNL("[", pad(i), i, ":g]");
                        }
                        else
                        {
                            // bad condition
                            LogNNL("[", pad(i), i, ":b]");
                        }

                        maybeNl(i);
                    }
                }
            }
        }

        LogNL();

        PAL.Fatal("heap");
    }

private:
    // If a given block is null, it's free until the next block that isn't
    // If a given block isn't null, it is using n blocks

    struct MemUsage
    {
        T *addr_ = nullptr;
        uint8_t n_ = 0;
    };

    struct alignas(T) AlignedBuffer
    {
        uint8_t buf[sizeof(T)];
    };

    std::array<MemUsage, POOL_SIZE> memUsageList_;
    std::array<AlignedBuffer, POOL_SIZE> memList_;
};


inline static void TestIsrPoolHeapAllocator()
{
    {
        // single-entry tests
        IsrPoolHeapAllocator<uint8_t, 3> alloc;

        auto p1 = alloc.allocate(1);
        alloc.Validate("add when first slot free");
        LogNL();

        auto p2 = alloc.allocate(1);
        alloc.Validate("add when second slot free");
        LogNL();

        auto p3 = alloc.allocate(1);
        (void)p3;
        alloc.Validate("add when last slot free (and also only slot free)");
        LogNL();

        auto p4 = alloc.allocate(1);
        (void)p4;
        alloc.Validate("add when none free");
        LogNL();


        alloc.deallocate(p1, 1);
        alloc.Validate("add when first slot free only (post removal)");
        p1 = alloc.allocate(1);
        alloc.Validate("add when first slot free only (post allocation)");
        LogNL();


        alloc.deallocate(p2, 1);
        alloc.Validate("add when second slot free only (post removal)");
        p2 = alloc.allocate(1);
        alloc.Validate("add when second slot free only (post allocation)");
        LogNL();


        alloc.deallocate((uint8_t *)5, 1);
        alloc.Validate("deallocate unknown pointer");
        LogNL();


        alloc.deallocate(nullptr, 1);
        alloc.Validate("deallocate nullptr");
        LogNL();
    }

    {
        // multi-entry tests
        // relies on above code working (validated by human)
        IsrPoolHeapAllocator<uint8_t, 6> alloc;

        auto p1 = alloc.allocate(2);
        alloc.Validate("add 2 at front");
        // 11----
        LogNL();


        auto p2 = alloc.allocate(1);
        alloc.Validate("add 1 in middle");
        // 112---
        LogNL();


        auto p3 = alloc.allocate(2);
        alloc.Validate("add 2 in middle");
        // 11233-
        LogNL();


        auto p4 = alloc.allocate(2);
        (void)p4;
        alloc.Validate("add 2 no fit");
        // 11233-
        LogNL();


        auto p5 = alloc.allocate(1);
        // 112335
        alloc.deallocate(p3, 2);
        // 112--5
        auto p6 = alloc.allocate(1);
        // 1126-5
        alloc.deallocate(p2, 1);
        // 11-6-5
        alloc.deallocate(p5, 1);
        // 11-6--
        alloc.Validate("add 2 initially blocked (before)");
        auto p7 = alloc.allocate(2);
        // 11-677
        alloc.Validate("add 2 initially blocked (after)");
        LogNL();


        alloc.deallocate(p6, 1);
        // 11--77
        alloc.Validate("add 2 middle (before)");
        auto p8 = alloc.allocate(2);
        // 118877
        alloc.Validate("add 2 middle (after)");
        LogNL();


        alloc.deallocate(p1, 2);
        // --8877
        alloc.deallocate(p8, 2);
        // ----77
        alloc.deallocate(p7, 2);
        // ------
        alloc.Validate("clear all");
        LogNL();
    }
}



