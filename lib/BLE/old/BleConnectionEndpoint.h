#pragma once

#include <vector>
#include <string>
using namespace std;

#include "Utl.h"


/////////////////////////////////////////////////////////////////////
// Connection Endpoint Base
/////////////////////////////////////////////////////////////////////


class BleConnectionEndpoint
{
public:
    enum class Phy : uint8_t
    {
        PHY_ANY = 0,
        PHY_1M = 1,
        PHY_2M = 2,
        PHY_S2 = 3,
        PHY_S8 = 4,
    };

    /////////////////////////////////////////////////////////////////////
    // Helper functions
    /////////////////////////////////////////////////////////////////////

    // Unrecognized strings return as PHY_ANY
    static Phy StrToPhy(string phyStr)
    {
        Phy phy = Phy::PHY_ANY;

        if      (phyStr == "1M") { phy = Phy::PHY_1M; }
        else if (phyStr == "2M") { phy = Phy::PHY_2M; }
        else if (phyStr == "S2") { phy = Phy::PHY_S2; }
        else if (phyStr == "S8") { phy = Phy::PHY_S8; }
        else                     { phy = Phy::PHY_ANY; }

        return phy;
    }

    static string PhyToStr(Phy phy)
    {
        string phyStr;

        switch (phy)
        {
            case Phy::PHY_1M: phyStr = "1M"; break;
            case Phy::PHY_2M: phyStr = "2M"; break;
            case Phy::PHY_S2: phyStr = "S2"; break;
            case Phy::PHY_S8: phyStr = "S8"; break;
            // also Phy::PHY_ANY
            default:          phyStr = "ANY"; break;
        }

        return phyStr;
    }

    static string PhyEnumToStr(uint8_t phyEnum)
    {
        string retVal;

        switch (phyEnum)
        {
            case BT_GAP_LE_PHY_1M   : retVal = "1M"; break;
            case BT_GAP_LE_PHY_2M   : retVal = "2M"; break;
            case BT_GAP_LE_PHY_CODED: retVal = "Sx"; break;
            default                 : retVal = "??"; break;
        }

        return retVal;
    }

    static vector<Phy> PhyStrListToPhyList(vector<string> phyStrList)
    {
        vector<Phy> phyList;

        for (auto &phyStr : phyStrList)
        {
            phyList.push_back(StrToPhy(phyStr));
        }

        return phyList;
    }


    /////////////////////////////////////////////////////////////////////
    // Initialization
    /////////////////////////////////////////////////////////////////////

    BleConnectionEndpoint()
    {
        SetConnectSecurityLevel(BT_SECURITY_L1);
    }

    const char *GetAddr()
    {
        return "tbd";
    }

    void SetName(string name)
    {
        name_ = name;
    }

    const string &GetName()
    {
        return name_;
    }

    void SetConnectSecurityLevel(bt_security_t securityLevel)
    {
        securityLevel_ = securityLevel;
    }

    bt_security_t GetConnectSecurityLevel()
    {
        return securityLevel_;
    }

    void SetEncryptionKey(unsigned int key)
    {
        encryptionKey_ = key;
    }

    unsigned int GetEncryptionKey()
    {
        return encryptionKey_;
    }

    void SetMaxConnections(uint8_t maxConnections)
    {
        maxConnections_ = maxConnections;
    }

    uint8_t GetMaxConnections()
    {
        return maxConnections_;
    }

    void SetPhyPreferences(string rxPhyList, string txPhyList)
    {
        rxPhyList_ = PhyStrListToPhyList(Split(rxPhyList));
        txPhyList_ = PhyStrListToPhyList(Split(txPhyList));
    }

    const vector<Phy> &GetRxPhyList()
    {
        return rxPhyList_;
    }

    const vector<Phy> &GetTxPhyList()
    {
        return txPhyList_;
    }

    void SetTxPowerConnections(double pct)
    {
        txPowerConn_ = pct;
    }

    double GetTxPowerConnections()
    {
        return txPowerConn_;
    }


private:

    string name_ = "peripheral";

    bt_security_t securityLevel_ = BT_SECURITY_L1;

    unsigned int encryptionKey_ = 0;

    uint8_t maxConnections_ = 1;

    vector<Phy> rxPhyList_ = { Phy::PHY_ANY };
    vector<Phy> txPhyList_ = { Phy::PHY_ANY };

    double txPowerConn_ = 100;
};


