#pragma once

#include <vector>
#include <utility>
using namespace std;

#include <bluetooth/hci_vs.h>

#include "Ble.h"
#include "Utl.h"


// https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/bluetooth/hci_pwr_ctrl
// https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/hci_pwr_ctrl/src/main.c
// https://devzone.nordicsemi.com/f/nordic-q-a/90133/how-to-set-tx-power-to-maximum-8dbm-for-advertising-as-well-as-when-connected


class BleHci
{
public:
    static int SetTxPowerAdv(double pct)
    {
        int dBm = GetTxPowerDbmAtPct(pct);

        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, (int8_t)dBm);

        return dBm;
    }

    static int GetTxPowerAdv()
    {
        int8_t dBm;

        get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, &dBm);

        return dBm;
    }

    static int SetTxPowerConn(bt_conn *conn, double pct)
    {
        int dBm = 0;
        uint16_t handle;

        int ret = bt_hci_get_conn_handle(conn, &handle);

        if (ret)
        {
            Log("No connection handle (err ", ret, ")");
        }
        else
        {
            dBm = GetTxPowerDbmAtPct(pct);

            set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, (int8_t)dBm);

            // Can also do this seemingly
            // set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
            //             handle,
            //             BT_HCI_VS_LL_TX_POWER_LEVEL_NO_PREF);
        }

        return dBm;
    }

    static int GetTxPowerConn(bt_conn *conn)
    {
        int8_t dBm = 0;
        uint16_t handle;

        int ret = bt_hci_get_conn_handle(conn, &handle);

        if (ret)
        {
            Log("No connection handle (err ", ret, ")");
        }
        else
        {
            get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, &dBm);
        }

        return dBm;
    }

    static int GetRssi(bt_conn *conn)
    {
        int8_t rssi = 0xFF;
        uint16_t handle;

        int ret = bt_hci_get_conn_handle(conn, &handle);

        if (ret)
        {
            Log("No connection handle (err ", ret, ")");
        }
        else
        {
            read_conn_rssi(handle, &rssi);
        }

        return rssi;
    }

private:

    static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
    {
        struct bt_hci_cp_vs_write_tx_power_level *cp;
        struct bt_hci_rp_vs_write_tx_power_level *rp;
        struct net_buf *buf, *rsp = NULL;
        int err;

        buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
                    sizeof(*cp));
        if (!buf) {
            Log("Unable to allocate command buffer");
            return;
        }

        cp = (bt_hci_cp_vs_write_tx_power_level *)net_buf_add(buf, sizeof(*cp));
        cp->handle = sys_cpu_to_le16(handle);
        cp->handle_type = handle_type;
        cp->tx_power_level = tx_pwr_lvl;

        err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
                    buf, &rsp);
        if (err) {
            uint8_t reason = rsp ?
                ((struct bt_hci_rp_vs_write_tx_power_level *)
                rsp->data)->status : 0;
            Log("Set Tx power err: ", err, " reason ", reason);
            return;
        }

        rp = (bt_hci_rp_vs_write_tx_power_level *)rsp->data;
        // Log("Actual Tx Power: ", rp->selected_tx_power);

        net_buf_unref(rsp);
    }

    static void get_tx_power(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl)
    {
        struct bt_hci_cp_vs_read_tx_power_level *cp;
        struct bt_hci_rp_vs_read_tx_power_level *rp;
        struct net_buf *buf, *rsp = NULL;
        int err;

        *tx_pwr_lvl = 0xFF;
        buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
                    sizeof(*cp));
        if (!buf) {
            Log("Unable to allocate command buffer");
            return;
        }

        cp = (bt_hci_cp_vs_read_tx_power_level *)net_buf_add(buf, sizeof(*cp));
        cp->handle = sys_cpu_to_le16(handle);
        cp->handle_type = handle_type;

        err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
                    buf, &rsp);
        if (err) {
            uint8_t reason = rsp ?
                ((struct bt_hci_rp_vs_read_tx_power_level *)
                rsp->data)->status : 0;
            Log("Read Tx power err: ", err, " reason ", reason);
            return;
        }

        rp = (bt_hci_rp_vs_read_tx_power_level *)rsp->data;
        *tx_pwr_lvl = rp->tx_power_level;

        net_buf_unref(rsp);
    }

    static int GetTxPowerDbmAtPct(double pct)
    {
        vector<double> mwList;

        for (auto &[dBm, mW] : txPowerLevelList_)
        {
            mwList.push_back(mW);
        }

        int idx = GetIdxAtPct(mwList, pct);
        
        int retVal = txPowerLevelList_[idx].first;

        return retVal;
    }

    inline static const vector<pair<int,float>> txPowerLevelList_ = {
        // dBm, mW
        { -40, 0.0001 },
        { -30, 0.0010 },
        { -20, 0.0100 },
        { -16, 0.0251 },
        { -12, 0.0631 },
        {  -8, 0.1585 },
        {  -4, 0.3981 },
        {   0, 1.0000 },
        {   2, 1.5849 },
        {   3, 1.9953 },
        {   4, 2.5119 },
        {   5, 3.1623 },
        {   6, 3.9811 },
        {   7, 5.0119 },
        {   8, 6.3096 },
    };

    static void read_conn_rssi(uint16_t handle, int8_t *rssi)
    {
        struct net_buf *buf, *rsp = NULL;
        struct bt_hci_cp_read_rssi *cp;
        struct bt_hci_rp_read_rssi *rp;

        int err;

        buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
        if (!buf) {
            Log("Unable to allocate command buffer");
            return;
        }

        cp = (bt_hci_cp_read_rssi *)net_buf_add(buf, sizeof(*cp));
        cp->handle = sys_cpu_to_le16(handle);

        err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
        if (err) {
            uint8_t reason = rsp ?
                ((struct bt_hci_rp_read_rssi *)rsp->data)->status : 0;
            Log("Read RSSI err: ", err, " reason ", reason);
            return;
        }

        rp = (bt_hci_rp_read_rssi *)rsp->data;
        *rssi = rp->rssi;

        net_buf_unref(rsp);
    }
};



