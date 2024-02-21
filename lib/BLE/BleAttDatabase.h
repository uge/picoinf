#pragma once

#include <string>
#include <vector>
using namespace std;


class BleAttDatabase
{
public:

    BleAttDatabase(string deviceName);

    uint16_t AddPrimaryService(string uuidStr);
    void AddCharacteristic(string uuidStr, string propertiesStr, string value = "");
    void AddCharacteristic(string uuidStr, string propertiesStr, vector<uint8_t>);

    vector<uint8_t> GetDatabaseData();


private:

    uint16_t AddEntry(uint16_t flags, string uuidTypeStr, vector<uint8_t> valueByteList);

    static void ReadDatabaseData(uint8_t *buf);


private:

    uint16_t nextHandle_ = 1;

    vector<vector<uint8_t>> rowByteListList_;
};

