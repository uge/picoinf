#include "NMEAStringMaker.h"
#include "NMEAStringParser.h"

using namespace std;

#include "StrictMode.h"


string NMEAStringMaker::Make(const vector<string> &dataList)
{
    // build the data part
    string tmp;
    const char *sep = "";
    for (const string &data : dataList)
    {
        tmp += sep;
        tmp += data;

        sep = ",";
    }
    string checksum = NMEAStringParser::CalculateChecksum(tmp);

    // construct entire string
    string retVal;
    retVal += "$";
    retVal += tmp;
    retVal += "*";
    retVal += checksum;

    return retVal;
}