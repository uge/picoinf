#pragma once

#include <string>
#include <vector>


class NMEAStringMaker
{
public:

    // Everything between $ and *
    // msgType,data,data,...
    static std::string Make(const std::vector<std::string> &dataList);
};
