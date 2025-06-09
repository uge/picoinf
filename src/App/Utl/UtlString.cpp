#include "UtlString.h"

#include <cinttypes>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
using namespace std;

#include "StrictMode.h"




string StrUtl::PadLeft(uint64_t val, char padChar, uint8_t fieldWidthTotal)
{
    return PadLeft(to_string(val), padChar, fieldWidthTotal);
}

string StrUtl::PadLeft(string valStr, char padChar, uint8_t fieldWidthTotal)
{
    int lenDiff = (int)fieldWidthTotal - (int)valStr.length();
    if (lenDiff > 0)
    {
        string valStrTmp;

        while (lenDiff)
        {
            valStrTmp += padChar;
            --lenDiff;
        }
        valStrTmp += valStr;

        valStr = valStrTmp;
    }

    return valStr;
}

string StrUtl::PadRight(const string& str, char padChar, uint8_t fieldWidthTotal)
{
    if (str.size() >= fieldWidthTotal)
    {
        return str;
    }

    return str + std::string(fieldWidthTotal - str.size(), padChar);
}








// thanks to (but lightly modified from)
// https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring

// trim from end of string (right)
std::string& RTrim(std::string& s, string t)
{
    s.erase(s.find_last_not_of(t.c_str()) + 1);
    return s;
}

// trim from beginning of string (left)
std::string& LTrim(std::string& s, string t)
{
    s.erase(0, s.find_first_not_of(t.c_str()));
    return s;
}

// trim from both ends of string (right then left)
std::string& Trim(std::string& s, string t)
{
    return LTrim(RTrim(s, t), t);
}








bool IsPrefix(const string &prefix, const string &target)
{
    bool retVal = false;
    
    if (prefix.length() <= target.length())
    {
        retVal = !strncmp(prefix.c_str(), target.c_str(), prefix.length());
    }

    return retVal;
}













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
vector<string> Split(string str, string delim, bool trimmed, bool allowEmpty)
{
    vector<string> retVal;

    auto process = [&](string token){
        if (trimmed)
        {
            token = Trim(token);
        }

        if (!token.empty() || allowEmpty)
        {
            retVal.push_back(token);
        }
    };

    size_t pos = 0;
    string token;
    while ((pos = str.find(delim)) != string::npos) {
        token = str.substr(0, pos);

        process(token);

        str.erase(0, pos + delim.length());
    }

    process(str);

    return retVal;
}

// make a list of strings from a str, splitting by a character
// eg a "b c;" ; d => [(a "b c;"), (d)]
vector<string> SplitByCharQuoteAware(const string &str, char splitChar)
{
    vector<string> cmdList;

    string cmd;
    bool inQuote = false;

    for (char c : str)
    {
        if (c == '"')
        {
            inQuote = !inQuote;
        }

        if (c == splitChar && inQuote == false)
        {
            cmdList.push_back(cmd);
            cmd.clear();
        }
        else
        {
            cmd += c;
        }
    }

    if (cmd.length())
    {
        cmdList.push_back(cmd);
    }

    return cmdList;
}

vector<string> SplitQuotedString(const string &input)
{
    vector<std::string> retVal;

    string currentToken;
    bool insideQuotes = false;

    for (size_t i = 0; i < input.length(); ++i)
    {
        char ch = input[i];

        if (ch == '"')
        {
            if (insideQuotes)
            {
                // Closing quote
                if (currentToken.size() > 0) {
                    retVal.push_back(currentToken);
                    currentToken.clear();
                }
                insideQuotes = false;
            }
            else
            {
                // Opening quote
                if (currentToken.size() > 0) {
                    retVal.push_back(currentToken);
                    currentToken.clear();
                }
                insideQuotes = true;
            }
        }
        else if (std::isspace(ch) && !insideQuotes)
        {
            // Split on spaces, unless inside quotes
            if (!currentToken.empty())
            {
                retVal.push_back(currentToken);
                currentToken.clear();
            }
        }
        else
        {
            currentToken += ch;  // Add character to current token
        }
    }

    if (!currentToken.empty())
    {
        // Add last token if any
        retVal.push_back(currentToken);
    }

    return retVal;
}


string Join(const vector<string> &valList, const string &sep)
{
    string retVal;

    if (valList.size())
    {
        retVal = valList[0];
    }

    for (size_t i = 1; i < valList.size(); ++i)
    {
        retVal += sep;
        retVal += valList[i];
    }

    return retVal;
}





char *ToString(uint64_t num)
{
    // strlen(18446744073709551615) = 20, so we need 21 for the null
    static const uint8_t BUF_SIZE = 21;
    static char buf[BUF_SIZE];

    snprintf(buf, BUF_SIZE, "%" PRIu64, num);

    return buf;
}

string ToString(double val, int precision)
{
    ostringstream out;
    out << fixed << setprecision(precision) << val;
    return out.str();
}

string ToString(const vector<uint8_t> &byteList)
{
    string retVal;

    for (const auto &b : byteList)
    {
        retVal.push_back(b);
    }

    return retVal;
}




