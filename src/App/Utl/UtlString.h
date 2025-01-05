#pragma once

#include <string>
#include <vector>



class StrUtl
{
public:
    static std::string PadLeft(uint64_t val, char padChar, uint8_t fieldWidthTotal);
    static std::string PadLeft(std::string valStr, char padChar, uint8_t fieldWidthTotal);
    static std::string PadRight(const std::string& str, char padChar, uint8_t fieldWidthTotal);
};


// thanks to (but lightly modified from)
// https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring

// trim from end of string (right)
extern std::string& RTrim(std::string& s, std::string t = " \t\n\r\f\v");
// trim from beginning of string (left)
extern std::string& LTrim(std::string& s, std::string t = " \t\n\r\f\v");
// trim from both ends of string (right then left)
extern std::string& Trim(std::string& s, std::string t = " \t\n\r\f\v");




extern bool IsPrefix(const std::string &prefix, const std::string &target);





// str is any string, optionally with delimiters
// delim can be a string of any size
// trimmed means whitespace stripped from each side of each element
// allowEmpty is tough
// - if you split on whitespace, you don't want empty substrings
// - if you split on comma, you do want empty (individual cells)
// - I'm tuning the defaults to be for whitespace
// 
//
// thanks to (but lightly modified from)
// https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
extern std::vector<std::string> Split(std::string str,
                                      std::string delim      = std::string{" "},
                                      bool        trimmed    = true,
                                      bool        allowEmpty = false);

extern std::vector<std::string> SplitQuotedString(const std::string &input);

extern std::string Join(const std::vector<std::string> &valList, const std::string &sep);


extern char *ToString(uint64_t num);
extern std::string ToString(double val, int precision = 3);
extern std::string ToString(const std::vector<uint8_t> &byteList);




template <typename Container>
inline std::string ContainerToString(const Container& container)
{
    std::string retVal;

    retVal += "[";

    std::string sep = "";
    for (const auto &val : container)
    {
        retVal += sep;
        retVal += std::to_string(val);

        sep = ", ";
    }

    retVal += "]";

    return retVal;
}







