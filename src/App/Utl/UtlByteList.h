#pragma once

#include <string>
#include <vector>


template <typename T>
void Append(T &a, const T &b)
{
    a.insert(end(a), begin(b), end(b));
}

template <typename T>
void Append(T &a, const T &&b)
{
    Append(a, b);
}

template <typename T>
inline std::vector<uint8_t> ToByteList(const T &val)
{
    std::vector<uint8_t> byteList;

    uint8_t *p = (uint8_t *)&val;
    for (int i = 0; i < (int)sizeof(T); ++i)
    {
        byteList.push_back(p[i]);
    }

    return byteList;
}

template <>
inline std::vector<uint8_t> ToByteList(const std::string& str)
{
    std::vector<uint8_t> byteList;

    for (const auto &b : str)
    {
        byteList.push_back(b);
    }

    return byteList;
}
