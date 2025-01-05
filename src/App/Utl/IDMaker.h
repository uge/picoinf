#pragma once

#include <bitset>
#include <cstdint>
#include <utility>


template <uint8_t CAPACITY = 32>
class IDMaker
{
public:
    // Next available, not necessarily next highest
    std::pair<bool, uint8_t> GetNextId()
    {
        bool ok = bits_.count() != CAPACITY;
        uint8_t id = 0;

        if (ok)
        {
            for (uint8_t i = 0; i < CAPACITY; ++i)
            {
                if (bits_[i] == false)
                {
                    bits_[i] = true;
                    id = i;
                    break;
                }
            }
        }

        return { ok, id };
    }

    void ReturnId(uint8_t id)
    {
        if (id < CAPACITY)
        {
            bits_[id] = false;
        }
    }

    uint8_t GetSize()
    {
        uint8_t size = 0;

        for (uint8_t i = 0; i < CAPACITY; ++i)
        {
            if (bits_[i])
            {
                ++size;
            }
        }

        return size;
    }

    uint8_t GetCapacity()
    {
        return CAPACITY;
    }

    void Clear()
    {
        for (uint8_t i = 0; i < CAPACITY; ++i)
        {
            bits_[i] = false;
        }
    }

    class Iterator
    {
    public:

        uint8_t operator*()
        {
            return idx_;
        }

        bool operator!=(const Iterator &it)
        {
            return idx_ != it.idx_;
        }

        Iterator &operator++()
        {
            for (++idx_; idx_ < CAPACITY; ++idx_)
            {
                if (bits_[idx_])
                {
                    break;
                }
            }

            return *this;
        }

        Iterator(uint8_t idx, std::bitset<CAPACITY> &bits)
        : idx_(idx)
        , bits_(bits)
        {
            // nothing to do
        }


    private:

        uint8_t idx_;
        std::bitset<CAPACITY> &bits_;
    };

    Iterator begin()
    {
        // find the first index with a value set
        uint8_t i = 0;
        for (; i < CAPACITY; ++i)
        {
            if (bits_[i])
            {
                break;
            }
        }

        return { i, bits_ };
    }

    Iterator end()
    {
        return { CAPACITY, bits_ };
    }


private:
    std::bitset<CAPACITY> bits_;
};
