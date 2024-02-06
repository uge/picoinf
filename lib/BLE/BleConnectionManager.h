#pragma once

#include <vector>
#include <string>
#include <map>
using namespace std;

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>

#include "Shell.h"
#include "Timeline.h"
#include "Ble.h"
#include "BleHci.h"
#include "BleConnectionEndpoint.h"
#include "BleGatt.h"



/////////////////////////////////////////////////////////////////////
// Connection Manager
/////////////////////////////////////////////////////////////////////

class BleConnectionManager
{
private:

    /////////////////////////////////////////////////////////////////////
    // Constants
    /////////////////////////////////////////////////////////////////////

    static const uint8_t TX_MAX_LEN_REQ = 251;


    /////////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////////

    static string ConnAddrStr(bt_conn *conn)
    {
        return Ble::AddrToStr(bt_conn_get_dst(conn));
    }


public:

    /////////////////////////////////////////////////////////////////////
    // Pairing Authorization Callbacks
    /////////////////////////////////////////////////////////////////////

    static void OnPairAuthDisplay(struct bt_conn *conn, unsigned int passkey)
    {
        timeline_.Event(__func__);
        Log("OnPairAuthDisplay: Passkey for ", ConnAddrStr(conn), ": ", passkey);
    }

    static void OnPairAuthCancel(struct bt_conn *conn)
    {
        timeline_.Event(__func__);
        Log("OnPairAuthCancel: Canceled attempt from ", ConnAddrStr(conn));
    }


    /////////////////////////////////////////////////////////////////////
    // Pairing Outcome Callbacks
    /////////////////////////////////////////////////////////////////////
    
    static void OnPairingComplete(struct bt_conn *conn, bool bonded)
    {
        timeline_.Event(__func__);

        ConnectionData &ctcData = connMap_[conn];
        ctcData.securityLevelNow = bt_conn_get_security(conn);

        Log("OnPairingComplete - ", ConnAddrStr(conn), ": ", bonded);
        Log("  Far side securityNow(", ctcData.securityLevelNow, "), securityStart(", ctcData.securityLevelStart, "), securityReq(", ctcData.securityLevelReq, ")");
    }

    static void OnPairingFailed(struct bt_conn *conn, enum bt_security_err reason)
    {
        timeline_.Event(__func__);
        Log("OnPairingFailed: Bond failed ", ConnAddrStr(conn), ": ", reason);
    }

    static void OnBondDeleted(uint8_t id, const bt_addr_le_t *peer)
    {
        timeline_.Event(__func__);
        Log("OnBondDeleted");
    }


    /////////////////////////////////////////////////////////////////////
    // Connected Callbacks
    /////////////////////////////////////////////////////////////////////
    

    static void DumpConnectionInfo(bt_conn *conn)
    {
        ConnectionData &ctcData = connMap_[conn];

        bt_conn_info ci;
        bt_conn_get_info(conn, &ci);

        Log("Connection info - ", ConnAddrStr(conn));
        Log("Name            - ", ctcData.bce->GetName());
        Log("  role: ", ci.role);
        Log("  id: ", ci.id);
        Log("  le.src: ", Ble::AddrToStr(ci.le.src));
        Log("  le.dst: ", Ble::AddrToStr(ci.le.dst));
        Log("  le.local: ", Ble::AddrToStr(ci.le.local));
        Log("  le.remote: ", Ble::AddrToStr(ci.le.remote));
        Log("  le.interval: ", ci.le.interval);
        Log("  le.latency: ", ci.le.latency);
        Log("  le.timeout: ", ci.le.timeout);
        Log("  le.phy->rx_phy: ", ci.le.phy->rx_phy);
        Log("  le.phy->tx_phy: ", ci.le.phy->tx_phy);
        Log("  ci.le.data_len.tx_max_len: ", ci.le.data_len->tx_max_len);
        Log("  ci.le.data_len.tx_max_time: ", ci.le.data_len->tx_max_time);
        Log("  ci.le.data_len.rx_max_len: ", ci.le.data_len->rx_max_len);
        Log("  ci.le.data_len.rx_max_time: ", ci.le.data_len->rx_max_time);
        Log("  ----");
        Log("  addr: ", ctcData.addr);
        Log("  startTime: ", ctcData.startTime);
        Log("  attMtu: ", ctcData.attMtu);
        Log("  txMaxLenStart: ", ctcData.txMaxLenStart);
        Log("  txMaxLenNow: ", ctcData.txMaxLenNow);
        Log("  txMaxLenReq: ", ctcData.txMaxLenReq);
        Log("  rxMaxLenStart: ", ctcData.rxMaxLenStart);
        Log("  rxMaxLenNow: ", ctcData.rxMaxLenNow);
        Log("  securityLevelStart: ", ctcData.securityLevelStart);
        Log("  securityLevelNow: ", ctcData.securityLevelNow);
        Log("  securityLevelReq: ", ctcData.securityLevelReq);
    }

    static void OnConnected(bt_conn *conn, uint8_t err)
    {
        timeline_.Event(__func__);

        // legacy, called before advertisement
        // Ideally I would ignore this, but other functions can get called
        // and they accidentally create the connection data.

        // Incr reference count this connection since we're holding the handle
        bt_conn_ref(conn);

        // Keep track of connections
        connMap_[conn];
    }

    static void OnConnectedFromAdvertisement(bt_conn *conn,
                                             BleConnectionEndpoint *bce,
                                             function<void(bt_conn *)> cbFnOnDisconnect)
    {
        timeline_.Event(__func__);

        // Keep track of connections
        // here we look up the details again and will be overwriting them
        ConnectionData &ctcData = connMap_[conn];

        // Extract information about the other side
        bt_conn_info ci;
        bt_conn_get_info(conn, &ci);

        // Keep state about this connection
        ctcData = {
            .conn = conn,
            .bce = bce,
            .cbFnOnDisconnect = cbFnOnDisconnect,
            .addr = ConnAddrStr(conn),
            .startTime = PAL.Millis(),
            .attMtu = bt_gatt_get_mtu(conn),
            .txMaxLenStart = ci.le.data_len->tx_max_len,
            .txMaxLenNow = ci.le.data_len->tx_max_len,
            .txMaxLenReq = TX_MAX_LEN_REQ,
            .rxMaxLenStart = ci.le.data_len->rx_max_len,
            .rxMaxLenNow = ci.le.data_len->rx_max_len,
            .securityLevelStart = bt_conn_get_security(conn),
            .securityLevelNow = bt_conn_get_security(conn),
            .securityLevelReq = bce->GetConnectSecurityLevel(),
        };

        Log("Connected BCE(", ctcData.bce->GetName(), ", ", ctcData.addr, ") at ", ctcData.startTime, " ptr ", conn);
        Log("  Far side rx now(", ctcData.rxMaxLenNow, "), start(", ctcData.rxMaxLenStart, ")");
        Log("  Far side tx now(", ctcData.txMaxLenNow, "), start(", ctcData.txMaxLenStart, "), req(", ctcData.txMaxLenReq, ")");
        Log("  Far side securityNow(", ctcData.securityLevelNow, "), securityStart(", ctcData.securityLevelStart, "), securityReq(", ctcData.securityLevelReq, ")");
        
        // Ensure the security level
        bt_passkey_set(bce->GetEncryptionKey());
        bt_conn_set_security(conn, ctcData.securityLevelReq);

        // Request phy preferences
        ApplyPhy(conn, bce->GetRxPhyList(), bce->GetTxPhyList());

        // apply configured power level
        ApplyTxPowerConnection(conn, bce->GetTxPowerConnections());

        OnRxMaxChange();
    }

    static void OnDisconnected(struct bt_conn *conn, uint8_t reason)
    {
        timeline_.Event(__func__);

        // check out if we know about this connection
        if (connMap_.count(conn))
        {
            // Decr reference count
            bt_conn_unref(conn);

            // Stop tracking connection
            ConnectionData &ctcData = connMap_[conn];
            if (ctcData.bce)
            {
                Log("Disconnection from ", ctcData.bce->GetName(), " by (", ctcData.addr, ") started at ", ctcData.startTime, " reason ", reason);
            }
            else
            {
                Log("Disconnection from unknown (", conn, ") reason ", reason);
            }

            auto cbFnOnDisconnect = ctcData.cbFnOnDisconnect;
            connMap_.erase(conn);

            OnRxMaxChange();

            cbFnOnDisconnect(conn);
        }
    }

    /////////////////////////////////////////////////////////////////////
    // BLE Low-Level Callbacks
    /////////////////////////////////////////////////////////////////////

    static void OnMtuUpdated(bt_conn *conn, uint16_t tx, uint16_t rx)
    {
        timeline_.Event(__func__);

        Log("OnMtuUpdated - ", ConnAddrStr(conn));
        Log("Updated MTU: TX: ", tx, " RX: ", rx, " bytes");
    }

    // The other side wants to update the connection parameters.
    // Decline by returning false.
    // Accept by returning true.
    //   Can tune the parameters before returning true.
    static bool OnLeParamReq(bt_conn *conn, bt_le_conn_param *param)
    {
        timeline_.Event(__func__);

        #if 0
        Log("OnLeParamReq - ", ConnAddrStr(conn));
        Log("  interval_min: ", param->interval_min);
        Log("  interval_max: ", param->interval_max);
        Log("  latency     : ", param->latency);
        Log("  timeout     : ", param->timeout);
        #endif

        return true;
    }

    // connection parameters for an LE connection have been updated
    static void OnLeParamUpdated(bt_conn *conn,
                                 uint16_t interval,
                                 uint16_t latency,
                                 uint16_t timeout)
    {
        timeline_.Event(__func__);

        #if 0
        Log("OnLeParamUpdated - ", ConnAddrStr(conn));
        Log("  interval: ", interval);
        Log("  latency : ", latency);
        Log("  timeout : ", timeout);
        #endif
    }

    // a remote Identity Address has been resolved
    static void OnIdentityResolved(bt_conn *conn,
                                   const bt_addr_le_t *rpa,
                                   const bt_addr_le_t *identity)
    {
        timeline_.Event(__func__);

        #if 0
        Log("OnIdentityResolved - ", ConnAddrStr(conn));
        Log("  rpa     : ", Ble::AddrToStr(rpa));
        Log("  identity: ", Ble::AddrToStr(identity));
        #endif
    }

    // The security of a connection has changed.
    //
    // The security level of the connection can either have been increased
    // or remain unchanged.
    // 
    // An increased security level means that the pairing procedure has
    // been performed or the bond information from a previous connection
    // has been applied.
    //
    // If the security level remains unchanged this means that the
    // encryption key has been refreshed for the connection.
    static void OnSecurityChanged(bt_conn *conn,
                                  bt_security_t level,
                                  bt_security_err err)
    {
        timeline_.Event(__func__);
        #if 0
        Log("OnSecurityChanged - ", ConnAddrStr(conn));
        Log("  level: ", level);
        Log("  err  : ", err);
        #endif

        ConnectionData &ctcData = connMap_[conn];
        ctcData.securityLevelNow = level;

        Log("OnSecurityChanged - ", ConnAddrStr(conn));
        Log("  Far side securityNow(", ctcData.securityLevelNow, "), securityStart(", ctcData.securityLevelStart, "), securityReq(", ctcData.securityLevelReq, ")");
    }

    // Remote information has been retrieved from the peer
    static void OnRemoteInfoAvailable(bt_conn *conn,
                                      bt_conn_remote_info *remote_info)
    {
        timeline_.Event(__func__);

        #if 0
        Log("OnRemoteInfoAvailable - ", ConnAddrStr(conn));
        Log("  version   : ", remote_info->version);
        Log("  subversion: ", remote_info->subversion);
        Log("  mfr       : ", remote_info->manufacturer);
        Log("  features  : ", remote_info->le.features ? *remote_info->le.features : 0);
        #endif
    }

    // the PHY of the connection has changed
    static void OnLePhyUpdated(bt_conn *conn,
                               bt_conn_le_phy_info *param)
    {
        timeline_.Event(__func__);

        #if 1
        Log("OnLePhyUpdated - ", ConnAddrStr(conn));
        Log("  rx_phy: ", BleConnectionEndpoint::PhyEnumToStr(param->rx_phy));
        Log("  tx_phy: ", BleConnectionEndpoint::PhyEnumToStr(param->tx_phy));
        #endif
    }

    // the maximum Link Layer payload length or
    // transmission time has changed
    static void OnLeDataLenUpdated(bt_conn *conn,
                                   bt_conn_le_data_len_info *info)
    {
        timeline_.Event(__func__);

        #if 0
        Log("OnLeDataLenUpdated - ", ConnAddrStr(conn));
        Log("  rx_max_len : ", info->rx_max_len);
        Log("  rx_max_time: ", info->rx_max_time);
        Log("  tx_max_len : ", info->tx_max_len);
        Log("  tx_max_time: ", info->tx_max_time);
        #endif

        ConnectionData &ctcData = connMap_[conn];
        ctcData.txMaxLenNow = info->tx_max_len;
        ctcData.rxMaxLenNow = info->rx_max_len;

        Log("OnLeDataLenUpdated - ", ConnAddrStr(conn));
        Log("  Far side rx now(", ctcData.rxMaxLenNow, "), start(", ctcData.rxMaxLenStart, ")");
        Log("  Far side tx now(", ctcData.txMaxLenNow, "), start(", ctcData.txMaxLenStart, "), req(", ctcData.txMaxLenReq, ")");

        OnRxMaxChange();
    }

    /////////////////////////////////////////////////////////////////////
    // Internal callbacks not from zephyr
    /////////////////////////////////////////////////////////////////////

    // Update smallest buffer so we tell gatt, who avoids crashing out due to
    // notify blowing up
    static void OnRxMaxChange()
    {
        uint8_t newSmallest = 0;
        for (auto &[conn, ctcData]: connMap_)
        {
            uint16_t bufSize = ctcData.rxMaxLenNow - 7; // subtract header overhead

            if (newSmallest == 0 || bufSize < newSmallest)
            {
                newSmallest = bufSize;
            }
        }

        if (smallestSendDataLen_ != newSmallest)
        {
            Log("OnRxMaxChange - Smallest send buffer change from ", smallestSendDataLen_, " to ", newSmallest);
            smallestSendDataLen_ = newSmallest;

            Log("  (keeping limit of 20 though for now)");
            BleGatt::SetSmallestSendDataLen(min(smallestSendDataLen_, (uint8_t)20));
        }
    }


    /////////////////////////////////////////////////////////////////////
    // Initialization
    /////////////////////////////////////////////////////////////////////

    static void Init()
    {
        timeline_.Event(__func__);

        SetupFatalHandler();
        RegisterCallbacks();
        SetAppearance();
    }

    static void SetupFatalHandler()
    {
        PAL.RegisterOnFatalHandler("BleConnectionManager Fatal Handler", []{
            timeline_.ReportNow();
        });
    }

    static void Report()
    {
        timeline_.Report();
    }
    
    static void SetAppearance()
    {
        // Set as unknown
        bt_set_appearance(0x0000);
    }


private:

    static void TxMaxLenReq(bt_conn *conn)
    {
        bt_conn_le_data_len_param p;
        p.tx_max_len = TX_MAX_LEN_REQ;
        p.tx_max_time = BT_GAP_DATA_TIME_MAX;
        bt_conn_le_data_len_update(conn, &p);
    }

    static void ApplyTxPowerConnection(bt_conn *conn, double pct)
    {
        Log("Applying tx power setting to Connection");
        int dBmBefore = BleHci::GetTxPowerConn(conn);
        int dBmSet    = BleHci::SetTxPowerConn(conn, pct);
        int dBmAfter  = BleHci::GetTxPowerConn(conn);
        int rssi      = BleHci::GetRssi(conn);
        LogNNL("Conn was(", dBmBefore, " dBm)");
        LogNNL(", req(", pct, "% = ", dBmSet, " dBm)");
        LogNNL(", now(", dBmAfter, " dBm)");
        LogNL();
        Log("Conn rssi: ", rssi);
    }

    // If you provide different coded values for rx/tx, they're combined (sorry)
    // If you provide ANY and other values, the ANY is wiped out (ignored effectively)
    static void ApplyPhy(bt_conn *conn, vector<BleConnectionEndpoint::Phy> rxPhyList, vector<BleConnectionEndpoint::Phy> txPhyList)
    {
        // The API is asking you to fill out a bitmask of the PHYs you
        // would prefer.
        //
        // When you want coded, you can select which codings you prefer.

        struct bt_conn_le_phy_param param = { 0, 0, 0 };
        // uint16_t options;     /** Connection PHY options. */
        // uint8_t  pref_tx_phy; /** Bitmask of preferred transmit PHYs */
        // uint8_t  pref_rx_phy; /** Bitmask of preferred receive PHYs */

        // you can prefer any (none), or any combination (OR'd)
        // BT_CONN_LE_PHY_OPT_NONE = 0,
        // BT_CONN_LE_PHY_OPT_CODED_S2  = BIT(0),
        // BT_CONN_LE_PHY_OPT_CODED_S8  = BIT(1),

        // You can prefer any (none), or any combination (OR'd)
        // BT_GAP_LE_PHY_NONE                    = 0,
        // BT_GAP_LE_PHY_1M                      = BIT(0),
        // BT_GAP_LE_PHY_2M                      = BIT(1),
        // BT_GAP_LE_PHY_CODED                   = BIT(2)

        auto fn = [](BleConnectionEndpoint::Phy phy, uint16_t &options, uint8_t &pref_xx_phy){
            switch (phy)
            {
                case BleConnectionEndpoint::Phy::PHY_1M:
                    pref_xx_phy |= BT_GAP_LE_PHY_1M;
                break;
                case BleConnectionEndpoint::Phy::PHY_2M:
                    pref_xx_phy |= BT_GAP_LE_PHY_2M;
                break;
                case BleConnectionEndpoint::Phy::PHY_S2:
                    pref_xx_phy |= BT_GAP_LE_PHY_CODED;
                    options     |= BT_CONN_LE_PHY_OPT_CODED_S2;
                break;
                case BleConnectionEndpoint::Phy::PHY_S8:
                    pref_xx_phy |= BT_GAP_LE_PHY_CODED;
                    options     |= BT_CONN_LE_PHY_OPT_CODED_S8;
                break;
                default:    // also Phy::PHY_ANY
                    pref_xx_phy |= BT_GAP_LE_PHY_NONE;
                    options     |= BT_CONN_LE_PHY_OPT_NONE;
                break;
            }
        };

        for (auto phy : rxPhyList)
        {
            fn(phy, param.options, param.pref_rx_phy);
        }
        for (auto phy : txPhyList)
        {
            fn(phy, param.options, param.pref_tx_phy);
        }

        bt_conn_le_phy_update(conn, &param);
    }

    static int RegisterCallbacks()
    {
        int err = false;

        // Set up callbacks for pairing authorization
        static bt_conn_auth_cb pairAuthCallbacks;
        pairAuthCallbacks =
        {
            // Users will have to enter a code and have it match what I have
            // set as the code
            .passkey_display = OnPairAuthDisplay,
            .cancel          = OnPairAuthCancel,
        };
        err = !!bt_conn_auth_cb_register(&pairAuthCallbacks);
        if (err)
        {
            Log("Couldn't register pairing auth callbacks");
        }

        // Set up callbacks for pairing outcome
        static struct bt_conn_auth_info_cb pairOutcomeCallbacks;
        pairOutcomeCallbacks =
        {
            .pairing_complete = OnPairingComplete,
            .pairing_failed   = OnPairingFailed,
            .bond_deleted     = OnBondDeleted,
        };
        err = !!bt_conn_auth_info_cb_register(&pairOutcomeCallbacks);
        if (err)
        {
            Log("Couldn't register pairing outcome callbacks");
        }

        // Set up callbacks for connection
        static bt_conn_cb btConnCb = {
            .connected = OnConnected,
            .disconnected = OnDisconnected,
            .le_param_req = OnLeParamReq,
            .le_param_updated = OnLeParamUpdated,
            .identity_resolved = OnIdentityResolved,
            .security_changed = OnSecurityChanged,
            .remote_info_available = OnRemoteInfoAvailable,
            .le_phy_updated = OnLePhyUpdated,   // not seen yet
            .le_data_len_updated = OnLeDataLenUpdated,
        };

        bt_conn_cb_register(&btConnCb);

        static struct bt_gatt_cb gatt_callbacks = {
            .att_mtu_updated = OnMtuUpdated,
        };
        bt_gatt_cb_register(&gatt_callbacks);

        return err;
    }


private:

    inline static Timeline timeline_;

    struct ConnectionData
    {
        bt_conn *conn = nullptr;
        BleConnectionEndpoint *bce = nullptr;
        function<void(bt_conn *)> cbFnOnDisconnect = [](bt_conn *){};
        string addr;
        uint64_t startTime = 0;
        uint16_t attMtu = 0;
        uint16_t txMaxLenStart = 0;
        uint16_t txMaxLenNow = 0;
        uint16_t txMaxLenReq = 0;
        uint16_t rxMaxLenStart = 0;
        uint16_t rxMaxLenNow = 0;
        bt_security_t securityLevelStart = BT_SECURITY_L1;
        bt_security_t securityLevelNow = BT_SECURITY_L1;
        bt_security_t securityLevelReq = BT_SECURITY_L1;
    };
    inline static map<bt_conn *, ConnectionData> connMap_;

    inline static uint8_t smallestSendDataLen_ = 0;

    inline static uint8_t maxConnections_ = 1;
    inline static uint16_t currConnections_ = 0;



private:


    /////////////////////////////////////////////////////////////////////
    // Shell
    /////////////////////////////////////////////////////////////////////

    static int Disconnect(bt_conn *conn)
    {
        timeline_.Event(__func__);

        return bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    static void SetConnectSecurityLevel(bt_conn *conn, bt_security_t securityLevel)
    {
        bt_conn_set_security(conn, securityLevel);
    }

    static void ApplyTxPowerToAllConnections(double pct)
    {
        for (auto &[conn, ctcData]: connMap_)
        {
            Log(ctcData.addr);
            ApplyTxPowerConnection(conn, pct);
        }
    }


public:

    static void SetupShell()
    {
        Shell::AddCommand("ble.conn.frames.set", [](vector<string> argList){
            Log("Updating ", connMap_.size(), " connections");
            for (auto &[conn, ctcData] : connMap_)
            {
                TxMaxLenReq(conn);
            }
        }, { .argCount = 0, .help = "Req fat frames from all connections" });

        Shell::AddCommand("ble.conn.frames.get", [](vector<string> argList){
            Log("Getting ", connMap_.size(), " connections");
            for (auto &[conn, ctcData] : connMap_)
            {
                bt_conn_info ci;
                bt_conn_get_info(conn, &ci);

                Log(ctcData.addr, ": rx(", ci.le.data_len->rx_max_len, "), tx(", ci.le.data_len->tx_max_len, ")");
            }
        }, { .argCount = 0, .help = "Req fat frames from all connections" });

        Shell::AddCommand("ble.conn.show", [](vector<string> argList){
            Log("Curr ", currConnections_, " / max ", maxConnections_);
            Log("Showing ", connMap_.size(), " connections");
            for (auto &[conn, ctcData] : connMap_)
            {
                DumpConnectionInfo(conn);
                LogNL();
            }
        }, { .argCount = 0, .help = "Req fat frames from all connections" });

        Shell::AddCommand("ble.conn.kick", [](vector<string> argList){
            Log("Kicking ", connMap_.size(), " connections");
            for (auto &[conn, ctcData] : connMap_)
            {
                Disconnect(conn);
                LogNL();
            }
        }, { .argCount = 0, .help = "Kick all existing connections" });

        Shell::AddCommand("ble.conn.security", [](vector<string> argList){
            if (argList.size() == 0)
            {
                for (auto &[conn, ctcData] : connMap_)
                {
                    Log("Security ", ctcData.bce->GetConnectSecurityLevel());
                }
            }
            else
            {
                uint8_t securityLevel = (uint8_t)atoi(argList[0].c_str());

                for (auto &[conn, ctcData] : connMap_)
                {
                    Log("Security ", ctcData.bce->GetConnectSecurityLevel(), " -> ", argList[0]);
                    ctcData.bce->SetConnectSecurityLevel((bt_security_t)securityLevel);
                    SetConnectSecurityLevel(conn, ctcData.bce->GetConnectSecurityLevel());
                }
            }
        }, { .argCount = -1, .help = "See/Set security" });

        Shell::AddCommand("ble.conn.phy", [](vector<string> argList){
            if (argList.size() == 0)
            {
                for (auto &[conn, ctcData] : connMap_)
                {
                    bt_conn_info ci;
                    bt_conn_get_info(conn, &ci);

                    Log(ConnAddrStr(conn));
                    Log("  rxPhy: ", BleConnectionEndpoint::PhyEnumToStr(ci.le.phy->rx_phy));
                    Log("  txPhy: ", BleConnectionEndpoint::PhyEnumToStr(ci.le.phy->tx_phy));
                }
            }
            else if (argList.size() == 2)
            {
                vector<BleConnectionEndpoint::Phy> rxPhyList = BleConnectionEndpoint::PhyStrListToPhyList(Split(argList[0]));
                vector<BleConnectionEndpoint::Phy> txPhyList = BleConnectionEndpoint::PhyStrListToPhyList(Split(argList[1]));

                LogNNL("rxPhyList:");
                for (auto &phy : rxPhyList)
                {
                    LogNNL(" ", BleConnectionEndpoint::PhyToStr(phy));
                }
                LogNL();

                LogNNL("txPhyList:");
                for (auto &phy : txPhyList)
                {
                    LogNNL(" ", BleConnectionEndpoint::PhyToStr(phy));
                }
                LogNL();

                for (auto &[conn, ctcData]: connMap_)
                {
                    Log("Setting for ", ctcData.addr);
                    ApplyPhy(conn, rxPhyList, txPhyList);
                }
            }
        }, { .argCount = -1, .help = "See/Set phy for connections" });

        Shell::AddCommand("ble.conn.pwr", [](vector<string> argList){
            Log(connMap_.size(), " connections");

            if (argList.size() == 0)
            {
                for (auto &[conn, ctcData]: connMap_)
                {
                    Log(ctcData.addr);
                    int dBm  = BleHci::GetTxPowerConn(conn);
                    int rssi = BleHci::GetRssi(conn);
                    Log("Conn dBm : ", dBm);
                    Log("Conn rssi: ", rssi);
                    LogNL();
                }
            }
            else if (argList.size() == 1)
            {
                double pct = atof(argList[0].c_str());
                ApplyTxPowerToAllConnections(pct);
            }
        }, { .argCount = -1, .help = "See dBm / Set tx pct pwr for connections" });

        Shell::AddCommand("ble.conn.pwr.ramp", [](vector<string> argList){
            string  type     = argList[0];   // adv / conn
            double  pctStart = atof(argList[1].c_str());
            double  pctEnd   = atof(argList[2].c_str());
            double  stepSize = atof(argList[3].c_str());
            uint8_t stepMs   = atoi(argList[4].c_str());

            if (type == "adv" || type == "conn")
            {
                double pct = pctStart;
                while (pct <= pctEnd)
                {
                    if (type == "adv")
                    {
                        Shell::Eval(string{"pwradv "} + to_string(pct));
                    }

                    PAL.Delay(stepMs);

                    pct += stepSize;
                }
            }
        }, { .argCount = 5, .help = "Ramp power levels" });

        Shell::AddCommand("ble.conn.report", [](vector<string> argList){
            Report();
        }, { .argCount = 0, .help = "Report" });
    }
};


