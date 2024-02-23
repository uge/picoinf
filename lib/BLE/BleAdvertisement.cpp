#include "BleAdvertisingDataFormatter.h"
#include "BleAdvertisement.h"

#include "StrictMode.h"


static vector<uint8_t> byteList;

const vector<uint8_t> &BleAdvertisement::GetRawAdvertisingDataStructure(string type)
{
    BleAdvertisingDataFormatter f;

    Log("Configuring ", type);

    bool ok;

    ok = f.AddFlags();
    
    const string &name = GetName();
    if (name.size())
    {
        ok = f.AddName(name);
        Log("  Adding name: \"", name, "\"", ok ? "" : " (ERR)");
    }

    const vector<string> &uuidList = GetAdvertisingUuidList();
    if (uuidList.size())
    {
        ok = f.AddUuidList(uuidList);
        Log("  Adding UUIDs: ", uuidList, ok ? "" : " (ERR)");
    }

    const vector<uint8_t> &mfrData = GetAdvertisingMfrData();
    if (mfrData.size())
    {
        f.AddMfrData(mfrData);
        Log("  MFR Data (", mfrData.size(), "): ", mfrData, ok ? "" : " (ERR)");
    }

    const string &webAddress = GetAdvertisingWebAddress();
    if (webAddress.size())
    {
        f.AddWebAddress(webAddress);
        Log("  Adding Web Address: https://", webAddress, ok ? "" : " (ERR)");
    }

    byteList = f.GetData();

    return byteList;
}