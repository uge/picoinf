#pragma once

#include "Log.h"

#include <cstdint>
#include <vector>


template <typename T>
class CircularBuffer
{
public:
    static const uint32_t DEFAULT_CAPACITY = 25;

    CircularBuffer()
    {
        SetCapacity(DEFAULT_CAPACITY);
    }

    CircularBuffer(uint32_t capacity)
    {
        SetCapacity(capacity);
    }

    void SetCapacity(uint32_t capacity)
    {
        capacity_ = capacity;

        Clear();

        list_.reserve(capacity_);
    }

    uint32_t GetCapacity()
    {
        return capacity_;
    }

    void Clear()
    {
        size_ = 0;
        head_ = 0;
        next_ = 0;
        writes_ = 0;
        overwrites_ = 0;
    }

    uint32_t Size()
    {
        return size_;
    }

    uint32_t Writes()
    {
        return writes_;
    }

    uint32_t Overwrites()
    {
        return overwrites_;
    }

    void PushBack(T &&val)
    {
        PushBack(val);
    }

    void PushBack(T &val)
    {
        list_[next_] = val;

        ++writes_;

        // did you overwrite the head?
        if (next_ == head_)
        {
            if (size_ == 0)
            {
                // no, there's no size yet, so this is the initial state
                ++size_;
            }
            else
            {
                // yes, push head along
                head_ = (head_ + 1) % capacity_;

                ++overwrites_;
            }
        }
        else
        {
            // no, so increment size
            ++size_;
        }

        // move next to the next slot
        next_ = (next_ + 1) % capacity_;
    }

    void PopFront()
    {
        if (size_)
        {
            head_ = (head_ + 1) % capacity_;

            --size_;
        }
    }

    T &operator[](uint32_t i)
    {
        static T badValue;

        if (i < size_)
        {
            return list_[(head_ + i) % capacity_];
        }
        else
        {
            return badValue;
        }
    }

    const T *Data()
    {
        return list_.data();
    }

    void Report(const char *str = nullptr)
    {
        if (str)
        {
            Log(str);
        }

        Log("size_ ", size_);
        Log("head_ ", head_);
        Log("next_ ", next_);
        Log("wrts_ ", writes_);
        Log("ovrw_ ", overwrites_);

        for (uint32_t i = 0; i < capacity_; ++i)
        {
            auto &val = list_[i];

            const char *hStr = "";
            const char *tStr = "";
            if (i == head_) { hStr = "H"; }
            if (i == next_) { tStr = "N"; }

            LogNNL("[", i, ":", hStr, tStr, ":", val, "]");
        }
        LogNL();

        for (uint32_t i = 0; i < Size(); ++i)
        {
            LogNNL("[", i, ":", operator[](i), "]");
        }
        LogNL();

        LogNL();
    }

private:
    std::vector<T> list_;

    uint32_t capacity_;
    uint32_t size_ = 0;
    uint32_t writes_ = 0;
    uint32_t overwrites_ = 0;

    uint32_t head_ = 0;
    uint32_t next_ = 0;
};

inline static void TestCircularBuffer()
{
    CircularBuffer<int> cb;

    cb.SetCapacity(3);
    cb.Report("initial state");

    cb.PushBack(1);
    cb.Report("one element in");
    cb.PopFront();
    cb.Report("one element removed");

    cb.PushBack(1);
    cb.Report("one element in");
    cb.PushBack(2);
    cb.Report("two element in");
    cb.PushBack(3);
    cb.Report("three element in");
    cb.PushBack(4);
    cb.Report("should be overwriting");

    cb.PopFront();
    cb.Report("pop");
    cb.PopFront();
    cb.Report("pop");
    cb.PopFront();
    cb.Report("pop");
    cb.PopFront();
    cb.Report("pop err");

    cb.PushBack(5);
    cb.Report("push");
 
    cb.SetCapacity(4);
    cb.Report("new capacity");

    for (int i = 0; i < 7; ++i)
    {
        cb.PushBack(i);
    }
    cb.Report("post-fill");

}




