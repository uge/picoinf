#include "Ble.h"
#include "BleAdvertiser.h"
#include "BleConnectionManager.h"
#include "BleBroadcaster.h"
#include "BlePeripheral.h"
#include "BleObserver.h"


void Ble::Init()
{
    BleAdvertiser::Init();
    BleConnectionManager::Init();
}

void Ble::SetupShell()
{
    BleAdvertiser::SetupShell();
    BleConnectionManager::SetupShell();
}

BlePeripheral &Ble::CreatePeripheral()
{
    peripheralList_.push_back({});

    BlePeripheral &retVal = peripheralList_[peripheralList_.size() - 1];

    return retVal;
}

BleBroadcaster &Ble::CreateBroadcaster()
{
    broadcasterList_.push_back({});

    BleBroadcaster &retVal = broadcasterList_[broadcasterList_.size() - 1];

    return retVal;
}

BleObserver &Ble::CreateObserver()
{
    observerList_.push_back({});

    BleObserver &retVal = observerList_[observerList_.size() - 1];

    return retVal;
}

void Ble::SetTxPowerAdvertising(uint8_t pct)
{
    if (started_)
    {
        BleAdvertiser::SetPowerAdv(pct);
    }
    else
    {
        Log("ERR: Ble cannot set adv power until after started");
    }
}

void Ble::Start()
{
    if (!started_)
    {
        // enable and load settings
        bt_enable(nullptr);
        settings_load();

        // tell gatt to do its thing
        BleGatt::Register();

        // tell advertiser list of peripherals so it can go build ads
        vector<pair<BleAdvertisementSource *, BleConnectionEndpoint *>> bleAdvertList;
        for (auto &bcast : broadcasterList_)
        {
            bleAdvertList.push_back({
                static_cast<BleAdvertisementSource *>(&bcast),
                nullptr,
            });
        }
        for (auto &periph : peripheralList_)
        {
            bleAdvertList.push_back({
                static_cast<BleAdvertisementSource *>(&periph),
                static_cast<BleConnectionEndpoint *>(&periph),
            });
        }
        BleAdvertiser::Register(bleAdvertList);

        // whatever else

        started_ = true;
    }
}

void Ble::Stop()
{
    if (started_)
    {
        started_ = false;
    }
}


/////////////////////////////////////////////////////////////////////
// Utilities
/////////////////////////////////////////////////////////////////////

string Ble::AddrToStr(const bt_addr_le_t *address)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(address, addr, sizeof(addr));

    return addr;
}


/////////////////////////////////////////////////////////////////////
// Storage
/////////////////////////////////////////////////////////////////////


vector<BlePeripheral>  Ble::peripheralList_;
vector<BleBroadcaster> Ble::broadcasterList_;
vector<BleObserver>    Ble::observerList_;


