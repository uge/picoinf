#pragma once

#include <cmath>
#include <vector>

#include "pico/rand.h"


// assumes list is sorted smallest/largest
template <typename T>
int GetIdxAtPct(std::vector<T> valList, double pct)
{
    int retVal = 0;

    if (!valList.empty())
    {
        T &smallest = valList[0];
        T &largest  = valList[valList.size() - 1];
        T range = largest - smallest;

        // Log("smallest: ", smallest);
        // Log("largest: ", largest);
        // Log("range: ", range);

        double pctClosest = (double)(valList[0] - smallest) / (double)range;
        int idxClosest = 0;

        for (int i = 0; i < (int)valList.size(); ++i)
        {
            T &val = valList[i];
            // Log("Checking val ", val);

            double pctThis = (double)(val - smallest) / (double)range * 100.0;

            double diffClosest = std::fabs(pct - pctClosest);
            double diffThis    = std::fabs(pct - pctThis);

            // Log("pctThis     ", pctThis);
            // Log("diffClosest ", diffClosest);
            // Log("diffThis    ", diffThis);

            if (diffThis <= diffClosest)
            {
                // Log("this is the new best");
                pctClosest = pctThis;
                idxClosest = i;
            }
            else
            {
                // getting further away from the answer
                break;
            }

            // LogNL();
        }

        retVal = idxClosest;
    }

    return retVal;
}


// https://docs.zephyrproject.org/3.0.0/reference/random/index.html
// range is inclusive
// mod operator is undefined on negatives, so have to treat specially
// [-10, -5]
// 10 - 5 + 1 = 6
// rand % 6 = [0-5]
// -5 - [0-5] = [-10--5]
// yup, works

// [-5, 5]
// 5 - -5 + 1 = 11
// rand % 11 = [0-10]
// -5 + [0-10] = [-5-5]
// yup works

// [-5, 0]
// 0 - -5 + 1 = 6
// rand % 6 = [0-5]
// -5 + [0-5] = [-5-0]
// yup, works

// [5, 10]
// 10 - 5 + 1 = 6
// rand % 6 = [0-5]
// 5 + [0-5] = [5-10]
// yup, works
inline uint32_t RandInRange(int32_t rangeLow, int32_t rangeHigh)
{
    int32_t retVal = 0;

    if (rangeLow <= rangeHigh)
    {
        if (rangeLow < 0 && rangeHigh < 0)
        {
            retVal = rangeLow - (get_rand_32() % (-rangeLow - -rangeHigh + 1));
        }
        else // rangeLow can be neg/0/pos, rangeHigh >= 0
        {
            retVal = rangeLow + (get_rand_32() % (rangeHigh - rangeLow + 1));
        }
    }

    return retVal;
}




template <typename T>
class Average
{
    static const uint16_t DEFAULT_MAX_SIZE = 10;
public:
    Average(uint16_t maxSize = DEFAULT_MAX_SIZE)
    : maxSize_(maxSize)
    {
        valList_.reserve(maxSize_);
    }

    T AddSample(T val)
    {
        if (valList_.size() < maxSize_)
        {
            valList_.push_back(val);

            sum_ += val;
        }
        else
        {
            sum_ -= valList_[idxNext_];

            valList_[idxNext_] = val;

            sum_ += val;

            idxNext_ = (idxNext_ + 1) % maxSize_;
        }

        average_ = round((double)sum_ / valList_.size());

        return average_;
    }

    T GetAverage()
    {
        return average_;
    }


private:
    std::vector<T> valList_;
    uint16_t maxSize_ = DEFAULT_MAX_SIZE;
    T sum_ = 0;
    uint16_t idxNext_ = 0;
    T average_ = 0;
};



template <typename T1, typename T2, typename T3>
inline T2 Clamp(const T1 low, const T2 val, const T3 high)
{
    T2 retVal = val;

    if (val < (T2)low)
    {
        retVal = (T2)low;
    }
    else if (val > (T2)high)
    {
        retVal = (T2)high;
    }

    return retVal;
}

template <typename T>
class Range
{
public:
    class Counter
    {
    public:
        Counter(T val, T step)
        : val_(val)
        , step_(step)
        {
            // Nothing to do
        }

        // prefix
        Counter &operator++()
        {
            val_ += step_;

            return *this;
        }

        operator T()
        {
            return val_;
        }

        T operator *()
        {
            return val_;
        }


    private:
        T val_;
        T step_;
    };

public:
    Range(T numStart, T numEnd)
    : numStart_(numStart)
    , numEnd_(numEnd)
    {
        // nothing to do
    }

    T GetStart() { return numStart_; }
    T GetEnd()   { return numEnd_;   }

    Counter begin()
    {
        if (numStart_ <= numEnd_)
        {
            return Counter(numStart_, 1);
        }
        else
        {
            return Counter(numStart_, -1);
        }
    }

    Counter end()
    {
        if (numStart_ <= numEnd_)
        {
            return Counter(numEnd_ + 1, 1);
        }
        else
        {
            return Counter(numEnd_ - 1, -1);
        }
    }

private:

    T numStart_;
    T numEnd_;
};

