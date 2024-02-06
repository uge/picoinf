#pragma once

#include <esb.h>

#include "Log.h"
#include "Evm.h"
#include "MpslSession.h"
#include "Timeline.h"
#include "KMessagePassing.h"


// Two modes of operation supported:
// - RX (default mode)
// - TX
//   - (only "no ack" implemented)
//
// When in TX mode:
// - not listening for inbound packets
// - can send packets
//   - will get callback when packet completes sending
//     - status can be sent ok or not sent ok
//   - attempting to send again wile prior packet sending will return error
// 
// When in RX mode:
// - listening for inbound packets
// - can send packets
//   - will temporarily switch to TX mode for the duration of the send
//     - meaning you will get a callback on TX complete
//     - system will automatically switch back to RX mode without intervention
//
// Switching modes on the fly is ok.
//
// Switching from TX to RX, while an outstanding TX operation is in progress
// will result in a callback to the TX complete handler indicating failure.
//
// Switching to TX from RX, where RX was temporarily in TX mode, will result
// in staying in TX mode, and the success of the outstanding TX packet will
// be returned correctly.
//   If you switch back to RX mode still before the TX completes will result
//   in the same behavior as a normal in-progress-TX to RX.
//
// MPSL is used to schedule these the operation of this system.
// Timeslot start/stop events are emitted.
//   Only during the timeslot can the Send() function be used.
//   If an outstanding TX is still outstanding at the end of the timeslot, a
//   notification event will be fired indicating failed TX.
//
// Start() will kick off the whole process of interacting with the events above.
// Stop() will immediately cease all operation
//
// The Mode can be changed at any time, whether started or not, whether in timeslot
// or not.
//
//
// ESB supports multiple pipes, message queues, RX/TX roles, and more.
// This class minimizes the complexity by:
// - no ptx/rtx roles as defined by esb, regarding acks, etc
//  - no ACKs supported, messages get sent once, additional sends happen at application layer
//  - instead, modes
//    - be in RX mode, but choose to send, no prob, temporary switch
//    - be in TX mode, send (faster), but never receive
// - addresses are uint16_t, no pipe designator supported (it is used as part of the address data internally)
//   - this means when sending, the base address and pipe are set to the destination, then tx, then revert
//   - gets you hw filtering
// - no queuing, 1 message rx or tx at a time, callbacks indicate when to tx again
// 
// 
class Esb
{
public:
    enum class Mode : uint8_t
    {
        RX = 1,
        TX = 2,
    };

    enum class Phy : uint8_t
    {
        NRF_1M = RADIO_MODE_MODE_Nrf_1Mbit,
        NRF_2M = RADIO_MODE_MODE_Nrf_2Mbit,
        BLE_1M = RADIO_MODE_MODE_Ble_1Mbit,
        BLE_2M = RADIO_MODE_MODE_Ble_2Mbit,
    };

    struct RxCfg
    {
        uint16_t addrRx  = 0;
        uint8_t  channel = 0;
        Phy      phy     = Phy::BLE_2M;
    };

    class Message
    {
        friend class Esb;

    public:
        inline static const uint8_t BUF_CAPACITY = CONFIG_ESB_MAX_PAYLOAD_LENGTH - 3;

        // for receiving
        uint8_t GetRssi()
        {
            return esbPayload_->rssi;
        }

        uint8_t GetBufSize()
        {
            return esbPayload_->length - 3;
        }

        // for receiving and sending
        uint8_t *GetBuf()
        {
            return esbPayload_->data + 3;
        }

        // for sending
        void SetUsedLength(uint8_t len)
        {
            esbPayload_->length = len + 3;
        }

    private:
        // User can't instantiate
        Message(esb_payload *esbPayload)
        : esbPayload_(esbPayload)
        {
            // nothing to do
        }

    private:
        esb_payload *esbPayload_ = nullptr;
    };

    static string PhyToStr(Phy phy)
    {
        string phyStr;

        switch (phy)
        {
            case Phy::NRF_1M: phyStr = "NRF_1M"; break;
            case Phy::NRF_2M: phyStr = "NRF_2M"; break;
            case Phy::BLE_1M: phyStr = "BLE_1M"; break;
            case Phy::BLE_2M: phyStr = "BLE_2M"; break;
            default:          phyStr = "?";      break;
        }

        return phyStr;
    }

    static Phy StrToPhy(string phyStr)
    {
        Phy phy = Phy::BLE_2M;

        if      (phyStr == "NRF_1M") { phy = Phy::NRF_1M; }
        else if (phyStr == "NRF_2M") { phy = Phy::NRF_2M; }
        else if (phyStr == "BLE_1M") { phy = Phy::BLE_1M; }
        else if (phyStr == "BLE_2M") { phy = Phy::BLE_2M; }

        return phy;
    }


public:

    Esb()
    {
        sess_.EnableTimeslotExtensions();
    }

    void SetMode(Mode mode)
    {
        // Log("ESB Mode change from ", Str(mode_), " to ", Str(mode));

        // If we're currently running, deal with effect of changing mode on the fly
        if (inTimeslot_)
        {
            Mode modeOld = mode_;

            mode_ = mode;

            if (modeOld != mode)
            {
                if (modeOld == Mode::TX && mode == Mode::RX)
                {
                    LiveModeChangeFromTxToRx();
                }
                if (modeOld == Mode::RX && mode == Mode::TX)
                {
                    LiveModeChangeFromRxToTx();
                }
            }
        }
        else
        {
            mode_ = mode;
        }
    }

    void SetCbOnTimeslotStart(function<void()> cbFnOnTimeslotStart)
    {
        cbFnOnTimeslotStart_ = cbFnOnTimeslotStart;
    }

    void SetCbOnTimeslotEnd(function<void()> cbFnOnTimeslotEnd)
    {
        cbFnOnTimeslotEnd_ = cbFnOnTimeslotEnd;
    }

    void SetCbOnRx(function<void(Message msg)> cbFnOnRx)
    {
        cbFnOnRx_ = cbFnOnRx;
    }

    void SetCbOnAssert(function<void()> cbFnOnAssert)
    {
        // forward it
        sess_.SetCbOnAssert(cbFnOnAssert);
    }

    // Only takes effect on start of each timeslot, which may be infinite
    // in duration.
    // Therefore the only safe way to be sure this takes effect is to apply
    // before Start.
    void SetAddrRx(uint16_t addr)
    {
        addrRx_ = addr;
    }

    // Only takes effect on start of each timeslot, which may be infinite
    // in duration.
    // Therefore the only safe way to be sure this takes effect is to apply
    // before Start.
    void SetAddrTx(uint16_t addr)
    {
        addrTx_ = addr;
    }

    // 0 - 50
    // Only takes effect on start of each timeslot, which may be infinite
    // in duration.
    // Therefore the only safe way to be sure this takes effect is to apply
    // before Start.
    void SetChannel(uint8_t channel)
    {
        channel_ = channel > 50 ? 50 : channel;
    }

    // 0 - 100
    // Only takes effect on start of each timeslot, which may be infinite
    // in duration.
    // Therefore the only safe way to be sure this takes effect is to apply
    // before Start.
    void SetTxPowerPct(uint8_t txPowerPct)
    {
        if (txPowerPct > 100) { txPowerPct = 100; }

        txPowerPct_ = txPowerPct;
    }

    // Only takes effect on start of each timeslot, which may be infinite
    // in duration.
    // Therefore the only safe way to be sure this takes effect is to apply
    // before Start.
    void SetPhy(Phy phy)
    {
        phy_ = phy;
    }

    void SetRxConfiguration(RxCfg rxCfg)
    {
        SetAddrRx(rxCfg.addrRx);
        SetChannel(rxCfg.channel);
        SetPhy(rxCfg.phy);
    }

    void EnableTimeslotExtensions()
    {
        sess_.EnableTimeslotExtensions();
    }

    void DisableTimeslotExtensions()
    {
        sess_.DisableTimeslotExtensions();
    }

    void Start()
    {
        timeline_.Event("ESB_START");

        if (!started_)
        {
            Log("ESB Start");
            Log("  mode    : ", Str(mode_));
            Log("  addrRx  : ", addrRx_);
            Log("  addrTx  : ", addrTx_);
            Log("  txPwrPct: ", txPowerPct_, "%");
            Log("  txPhy   : ", PhyToStr(phy_));
            Log("  chan    : ", channel_);
            // add interval details
            // add note about whether attempt to extend

            sess_.SetCbOnStart([this] { OnMpslTimeslotStart(); });
            sess_.SetCbOnEnd([this]   { OnMpslTimeslotEnd();   });

            // what to do here?
            sess_.SetCbOnNoMoreComing([this]{ Log("No More Coming"); });

            sess_.OpenSession();

            // Small sizes lets you fit in tight spots
            // We secretly know the MPSL system will kill us ~2'500 before we're done
            // So we ensure we have time enough for one worst-case-duration packet rx/tx
            static const uint64_t PERIOD_US   = 5'000;
            static const uint64_t DURATION_US = 5'000;
            sess_.RequestTimeslots(PERIOD_US, DURATION_US);

            started_ = true;
        }
    }

    void Stop()
    {
        timeline_.Event("ESB_STOP");

        if (started_)
        {
            // this guarantees to tear down current timeslot
            // without firing another TimeslotEnd event
            sess_.CloseSession();

            started_ = false;
        }
    }

    Message GetMessageToSend()
    {
        return Message(&esbPayloadTx_);
    }

    // this call is synchronous to send and returns a bool
    // true = send worked
    // false = it didn't
    bool Send(Message &msg)
    {
        timeline_.Event("ESB_SEND");

        bool txOk;

        if (inTimeslot_)
        {
            // maybe we have to change to TX first
            bool changeToRxAfter = false;
            if (mode_ == Mode::RX)
            {
                // We want a temporary state change
                timeline_.Event("TEMP_ROLE_CHANGE");
                SetMode(Mode::TX);
                changeToRxAfter = true;
            }

            // sync send
            int retVal = EsbTx(*msg.esbPayload_);

            // check return
            if (retVal == -2)
            {
                timeline_.Event("ESB_TX_TIMEOUT");
                txOk = false;
            }
            else if (retVal == -1)
            {
                timeline_.Event("ESB_TX_FAIL");
                txOk = false;
            }
            else if (retVal == 0)
            {
                timeline_.Event("ESB_TX_SEND_FN_FAIL");
                txOk = false;
            }
            else // (retVal == 1)
            {
                timeline_.Event("ESB_TX_SUCCESS");
                txOk = true;
            }

            // change back to RX if needed
            if (changeToRxAfter)
            {
                timeline_.Event("ESB_TEMP_ROLE_CHANGE_BACK");
                SetMode(Mode::RX);
            }

            stats_.txOk  += txOk ? 1 : 0;
            stats_.txBad += txOk ? 0 : 1;
        }
        else
        {
            timeline_.Event("ESB_SEND_NOT_IN_TIMESLOT");
            txOk = false;
            ++stats_.txReject;
        }

        return txOk;
    }
    
    void Report()
    {
        timeline_.Report();

        PrintStats();
    }

    void PrintStats()
    {
        static const uint8_t FIELD_WIDTH = 7;

        Log("timeslots     : ", StrUtl::PadLeft(Commas(stats_.timeslots),     ' ', FIELD_WIDTH));
        Log("tx            : ", StrUtl::PadLeft(Commas(stats_.tx),            ' ', FIELD_WIDTH));
        Log("txOk          : ", StrUtl::PadLeft(Commas(stats_.txOk),          ' ', FIELD_WIDTH));
        Log("txBad         : ", StrUtl::PadLeft(Commas(stats_.txBad),         ' ', FIELD_WIDTH));
        Log("txReject      : ", StrUtl::PadLeft(Commas(stats_.txReject),      ' ', FIELD_WIDTH));
        Log("rx            : ", StrUtl::PadLeft(Commas(stats_.rx),            ' ', FIELD_WIDTH));
        Log("rxDropped     : ", StrUtl::PadLeft(Commas(stats_.rxDropped),     ' ', FIELD_WIDTH));
        Log("rxNotify      : ", StrUtl::PadLeft(Commas(stats_.rxNotify),      ' ', FIELD_WIDTH));
        Log("rxNotifyLost  : ", StrUtl::PadLeft(Commas(stats_.rxNotifyLost),  ' ', FIELD_WIDTH));
    }


private:

    ///////////////////////////////////////////////////////////////////////////////
    // Timeslot interaction
    ///////////////////////////////////////////////////////////////////////////////

    void OnMpslTimeslotStart()
    {
        esbInstance_ = this;

        timeline_.Event("ESB_TIMESLOT_START");

        EsbStart();

        if (mode_ == Mode::RX)
        {
            StartModeRx();
        }
        else if (mode_ == Mode::TX)
        {
            StartModeTx();
        }

        inTimeslot_ = true;

        ++stats_.timeslots;

        cbFnOnTimeslotStart_();
    }

    void OnMpslTimeslotEnd()
    {
        esbInstance_ = nullptr;

        timeline_.Event("ESB_TIMESLOT_END");

        if (mode_ == Mode::RX)
        {
            StopModeRx();
        }
        else if (mode_ == Mode::TX)
        {
            StopModeTx();
        }

        EsbStop();

        inTimeslot_ = false;
        cbFnOnTimeslotEnd_();
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Mode state and Mode change
    ///////////////////////////////////////////////////////////////////////////////

    void StartModeRx()
    {
        timeline_.Event("ESB_START_RX");
        EsbConfigureAddresses();
        EsbStartRx();
    }

    void StopModeRx()
    {
        timeline_.Event("ESB_STOP_RX");
        EsbStopRx();
    }

    void StartModeTx()
    {
        timeline_.Event("ESB_START_TX");
        EsbConfigureAddresses();
    }

    void StopModeTx()
    {
        timeline_.Event("ESB_STOP_TX");
    }

    void LiveModeChangeFromTxToRx()
    {
        timeline_.Event("ESB_LIVE_TX_TO_RX");

        // fully change state
        StopModeTx();
        StartModeRx();
    }

    void LiveModeChangeFromRxToTx()
    {
        timeline_.Event("ESB_LIVE_RX_TO_TX");
        StopModeRx();
        StartModeTx();
    }

    ///////////////////////////////////////////////////////////////////////////////
    // ESB Library Callbacks (ISRs)
    ///////////////////////////////////////////////////////////////////////////////

    void OnEsbRx(esb_payload *esbPayload)
    {
        // how do I want to present the data upward?
        // too bad I have to copy it, but I think I do
        // evm.RegisterInterruptHandler([&, esbPayload]{
        Evm::QueueWork("ESB_QUEUE_EVM", [=, this]{
            if (inTimeslot_ && mode_ == Mode::RX)
            {
                ++stats_.rxNotify;

                Message msg(esbPayload);
                cbFnOnRx_(msg);
            }
            else
            {
                ++stats_.rxNotifyLost;
            }

            esbPayloadRxPool_.Return(esbPayload);
        });
    }


    ///////////////////////////////////////////////////////////////////////////////
    // ESB Library Interaction
    ///////////////////////////////////////////////////////////////////////////////

    // https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/intro-to-shockburstenhanced-shockburst
    // https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_esb.html
    // https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fradio.html
    // https://infocenter.nordicsemi.com/topic/ps_nrf52840/radio.html?cp=4_0_0_5_19_13_46#register.TXPOWER
    // https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fradio.html&anchor=register.FREQUENCY


    esb_tx_power CalculateEsbTxPower()
    {
        // ESB tries to limit you to +4dBm, but whatever value you put in here is
        // slotted into the radio, so you can get up to +8dBm in reality, the same
        // as BLE
        const vector<pair<uint32_t,float>> txPowerLevelList_ = {
            { RADIO_TXPOWER_TXPOWER_Neg40dBm, 0.0001 },
            { RADIO_TXPOWER_TXPOWER_Neg30dBm, 0.0010 },
            { RADIO_TXPOWER_TXPOWER_Neg20dBm, 0.0100 },
            { RADIO_TXPOWER_TXPOWER_Neg16dBm, 0.0251 },
            { RADIO_TXPOWER_TXPOWER_Neg12dBm, 0.0631 },
            { RADIO_TXPOWER_TXPOWER_Neg8dBm,  0.1585 },
            { RADIO_TXPOWER_TXPOWER_Neg4dBm,  0.3981 },
            { RADIO_TXPOWER_TXPOWER_0dBm,     1.0000 },
            { RADIO_TXPOWER_TXPOWER_Pos2dBm,  1.5849 },
            { RADIO_TXPOWER_TXPOWER_Pos3dBm,  1.9953 },
            { RADIO_TXPOWER_TXPOWER_Pos4dBm,  2.5119 },
            { RADIO_TXPOWER_TXPOWER_Pos5dBm,  3.1623 },
            { RADIO_TXPOWER_TXPOWER_Pos6dBm,  3.9811 },
            { RADIO_TXPOWER_TXPOWER_Pos7dBm,  5.0119 },
            { RADIO_TXPOWER_TXPOWER_Pos8dBm,  6.3096 },
        };

        vector<double> mwList;
        for (auto &[dBm, mW] : txPowerLevelList_)
        {
            mwList.push_back(mW);
        }

        int idx = GetIdxAtPct(mwList, txPowerPct_);
        
        uint32_t retVal = txPowerLevelList_[idx].first;

        return (esb_tx_power)retVal;
    }

    esb_bitrate CalculateEsbBitrate()
    {
        return (esb_bitrate)(uint8_t)phy_;
    }

    void EsbStart()
    {
        esb_bitrate bitrate  = CalculateEsbBitrate();
        esb_tx_power txPower = CalculateEsbTxPower();

        struct esb_config config = {
            .protocol           = ESB_PROTOCOL_ESB_DPL, // dynamic payload len
            .mode               = ESB_MODE_PTX,         // if you're PRX, you pre-write acks to auto-send on rx.  we don't want that functionality
            .event_handler      = EsbIsrEventHandler,   // handle callbacks
            .bitrate            = bitrate,              // user-selected
            .crc                = ESB_CRC_16BIT,        // maximum error redundancy
            .tx_output_power    = txPower,              // user-selected
            .retransmit_delay   = 600,                  // irrelevant and "wrong," should depend on time taken to send packet and get ack, ignore for now
            .retransmit_count   = 0,                    // don't retransmit, needed even though we .noack=true, according to spec
            .tx_mode            = ESB_TXMODE_AUTO,      // Send any queued packets as soon as radio is free, no manual trigger needed
            .payload_length     = 0,                    // unused in dynamic payload length
            .selective_auto_ack = true,                 // honor the .noack field on inbound msgs (as in, don't force-ack when not rquested)
        };

        int err;
        
        err = esb_init(&config);
        if (err)
        {
            Log("ERR Esb: Could not init: ", err);
        }

        EsbConfigureAddresses();

        EsbSetChannel();
    }

    void EsbConfigureAddresses()
    {
        // ESB, and the radio module itself really, support the notion of
        // addresses and pipes.
        //
        // The "base address" (32-bit) serves as your local rx and remote tx address.
        // The "pipe" refers to a specific "port" on that address.
        //
        // The HW will filter out inbound traffic based on these configurations.
        //
        // There are 8 pipes supported.
        //
        // Interestingly, there are two base addresses, though.
        // - base address 0, which supports pipe 0 (only)
        // - base address 1, which supports pipe 1-7
        //
        // When you TX, you only get to specify the port to send on/to.
        // 
        // Meaning, for:
        // - sending - you can have two different addresses that you're sending to
        //   depending the port you choose to use.
        // - receiving - whoever sent to you had to select the address and port for
        //   you to see this.
        //
        // Thusly, the address format is: [b1][b2][b3][b4][p]
        // There are restrictions
        // - b1 cannot be 0x55 or 0xAA
        // - b1 and b2 cannot be 0x00
        // - p cannot be 0x00
        // 
        // This whole scheme sucks though, so I'm simplifying it to be much simpler.
        // - eliminate the concept of base addresses and ports
        // - un-restrict receive and send addresses to be the same
        //   - use 16-bit addresses for each
        //
        // This is accomplished by:
        // - Consider [b3][b4] to be the 16-bit space you can configure
        // - Hard-code [b1][b2]..[p] to be compliant with restrictions
        // - When receiving, populate with local address
        // - When sending, populate with remote address
        //
        // The two base addresses will be the same.
        // The port IDs will be the same.
        // 

        uint16_t addr = mode_ == Mode::RX ? addrRx_ : addrTx_;
        // Log("Setting addr ", addr);

        uint8_t baseAddr[4] = {
            0xE7,
            0xE7,
            (uint8_t)((addr >> 8) & 0xFF),
            (uint8_t)((addr >> 0) & 0xFF),
        };

        int err;
        err = esb_set_base_address_0(baseAddr);
        if (err)
        {
            Log("ERR Esb: Could not set base addr 0: ", err);
        }

        err = esb_set_base_address_1(baseAddr);
        if (err)
        {
            Log("ERR Esb: Could not set base addr 1: ", err);
        }

        uint8_t pipeAddrList[8] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };
        err = esb_set_prefixes(pipeAddrList, sizeof(pipeAddrList));
        if (err)
        {
            Log("ERR Esb: Could not set prefixes: ", err);
        }
    }

    void EsbSetChannel()
    {
        // ESB docs say 0-100 supported (so 101 different), freq = 2400MHz + (n * 100MHz).
        // however, 
        // seems that you can select an additional 40 channels by flipping a particular bit
        // within the frequency value to trigger "LOW" addresss mode, where the meaning
        // is freq = 2360MHz + (n * 100MHz).
        // so in total, there's 161 frequencies to choose from.
        //
        // Oh but wait no nevermind they limit it to 0-100.  Fine.
        // Log("Setting channel ", channel_);

        // seemingly can still receive if +/- 1 channel, so limit users to select 0-50
        // and we map onto 0-100
        esb_set_rf_channel(channel_ * 2);
    }

    static void EsbStop()
    {
        esb_disable();
    }

    static void EsbStartRx()
    {
        int err = esb_start_rx();
        if (err)
        {
            Log("RX Start failed, err ", err);
        }
    }

    static bool EsbStopRx()
    {
        int err = esb_stop_rx();
        if (err)
        {
            // This happens every time since MPSL kills the radio
            // Log("RX Stop failed, err ", err);
        }

        return !err;
    }

    // 4 possible return values:
    // - -2 = send timeout
    // - -1 = send failed after ISR
    // -  0 = send function failed immediately
    // -  1 = send succeeded after ISR
    static int EsbTx(esb_payload &esbPayload)
    {
        int retVal;

        esb_flush_tx();

        esbPayload.pipe = 0;
        esbPayload.noack = true;

        // clear out ISR response flag
        txCompleteStatus_ = 0;

        // ensure no stray radio signals from prior timeslot have
        // incremented the semaphore
        sem_.Reset();

        int err = esb_write_payload(&esbPayload);
        if (err)
        {
            // this can happen if timeslot ends in the middle of a write
            retVal = 0;
        }
        else
        {
            // wait for ISR response flag

            // at slowest phy, largest packet, it takes 2,380us waiting for send
            // to complete.  round up to 2400.
            // this will only ever take this long when genuinely the
            // worst case scenario, or if the radio got shut off in MPSL
            // and an in-progress transmit dies and hence never signals back.
            if (sem_.Take(K_USEC(2'400)))
            {
                // success == 1, failure == -1
                retVal = txCompleteStatus_;
            }
            else
            {
                // timeout == -2
                retVal = -2;
            }
        }

        return retVal;
    }

    static void EsbIsrEventHandler(struct esb_evt const *event)
    {
        Esb *that = esbInstance_;

        if (that)
        {
            if (event->evt_id == ESB_EVENT_TX_SUCCESS)
            {
                that->timeline_.Event("ESB_EVENT_TX_SUCCESS");

                txCompleteStatus_ = 1;
                sem_.Give();
            }
            else if (event->evt_id == ESB_EVENT_TX_FAILED)
            {
                that->timeline_.Event("ESB_EVENT_TX_FAILED");

                txCompleteStatus_ = -1;
                sem_.Give();
            }
            else if (event->evt_id == ESB_EVENT_RX_RECEIVED)
            {
                that->timeline_.Event("ESB_EVENT_RX_RECEIVED");

                bool cont = true;
                while (cont)
                {
                    bool useIfReceived = true;

                    esb_payload *payload = that->esbPayloadRxPool_.Borrow();
                    if (payload == nullptr)
                    {
                        payload = &that->esbPayloadRxFailSafe_;

                        useIfReceived = false;
                    }

                    int retVal = esb_read_rx_payload(payload);

                    const char *evtStr = "ESB_RX_EVT_PKT_UNKNOWN";

                    if (retVal == 0)
                    {
                        that->stats_.rx += 1;
                        evtStr = "ESB_RX_EVT_PKT";

                        if (useIfReceived)
                        {
                            that->OnEsbRx(payload);
                        }
                        else
                        {
                            that->stats_.rxDropped += 1;
                            that->timeline_.Event("ESB_RX_PKT_DROPPED");
                        }
                    }
                    else
                    {
                        cont = false;

                        if (retVal == -EACCES)       { evtStr = "ESB_RX_EVT_PKT_EACCES";  }
                        else if (retVal == -EINVAL)  { evtStr = "ESB_RX_EVT_PKT_EINVAL";  }
                        else if (retVal == -ENODATA) { evtStr = "ESB_RX_EVT_PKT_ENODATA"; }

                        if (useIfReceived)
                        {
                            that->esbPayloadRxPool_.Return(payload);
                        }
                    }

                    that->timeline_.Event(evtStr);
                }
            }
        }
    }


private:
    Mode mode_ = Mode::RX;
    static const char *Str(Mode mode)
    {
        return mode == Mode::TX ? "TX" : "RX";
    }

    function<void()>        cbFnOnTimeslotStart_ = []{};
    function<void()>        cbFnOnTimeslotEnd_   = []{};
    function<void(Message)> cbFnOnRx_            = [](Message){};

    uint16_t addrRx_     = 0;
    uint16_t addrTx_     = 0;
    uint8_t  channel_    = 0;
    uint8_t  txPowerPct_ = 100;
    Phy      phy_        = Phy::BLE_2M;

    MpslSession sess_;
    bool inTimeslot_      = false;
    bool started_         = false;

    struct Stats
    {
        uint32_t timeslots     = 0;
        uint32_t tx            = 0;
        uint32_t txReject      = 0;
        uint32_t txOk          = 0;
        uint32_t txBad         = 0;
        uint32_t rx            = 0;
        uint32_t rxDropped     = 0;
        uint32_t rxNotify      = 0;
        uint32_t rxNotifyLost  = 0;
    };
    Stats stats_;

    Timeline timeline_;

    static const uint8_t BUF_RX_COUNT = 1;
    ObjectPool<esb_payload, BUF_RX_COUNT> esbPayloadRxPool_;
    esb_payload esbPayloadTx_;


    inline static Esb *esbInstance_ = nullptr;
    inline static esb_payload esbPayloadRxFailSafe_;

    inline static KSemaphore sem_;
    inline static volatile int txCompleteStatus_ = 0;





    ///////////////////////////////////////////////////////////////////////////////
    // Shell
    ///////////////////////////////////////////////////////////////////////////////

public:
    static void SetupShell()
    {
        static Esb esb;

        static bool inTimeslot = false;
        static TimedEventHandlerDelegate ted;
        static uint32_t txCount = 0;
        static uint32_t rxCount = 0;
        static uint32_t toSendCount = 0;
        static uint32_t toSendCountOrig = 0;
        static uint64_t toSendStartTime = 0;
        static vector<int> txFailList;
        static bool slotNotify = true;
        static bool rxNotify = true;
        static bool txNotify = false;
        static uint32_t seqNo = 0;
        struct Data
        {
            uint32_t seqNo;
            double   seqNoDouble;
        };
        static size_t sendSize = sizeof(Data);
        static auto fnSend = []{
            Message msg = esb.GetMessageToSend();

            Data *d = (Data *)msg.GetBuf();
            d->seqNo = seqNo;
            d->seqNoDouble = seqNo;

            msg.SetUsedLength(sendSize);
            bool retVal = esb.Send(msg);
            if (txNotify)
            {
                Log("TX(", retVal, ")");
            }

            if (retVal)
            {
                ++seqNo;
            }

            return retVal;
        };

        ted.SetCallback([]{
            if (toSendCount)
            {
                if (inTimeslot)
                {
                    if (fnSend())
                    {
                        ++txCount;
                        --toSendCount;

                        if (toSendCount)
                        {
                            ted.RegisterForTimedEvent(0);
                        }
                        else
                        {
                            uint64_t timeDiff = PAL.Micros() - toSendStartTime;
                            uint64_t ratePerSec = (double)toSendCountOrig / ((double)timeDiff / 1'000'000.0);
                            uint64_t usPerSend = timeDiff / toSendCountOrig;
                            Log("done - ", Commas(ratePerSec), "/sec, ", Commas(usPerSend), " us/packet, in ", Commas(timeDiff), " us");
                        }
                    }
                    else
                    {
                        txFailList.push_back(txCount);

                        if (txFailList.size() == 10)
                        {
                            LogNNL("FailList: ");
                            for (auto num : txFailList)
                            {
                                LogNNL(" ", num);
                            }
                            LogNL();

                            txFailList.clear();
                        }

                        ted.RegisterForTimedEvent(100);
                    }
                }
                else
                {
                    // wait for the slot start callback to register me
                }
            }
        }, "TIMER_ESB_SHELL_SEND");

        esb.SetCbOnTimeslotStart([&]{
            if (slotNotify || toSendCount)
            {
                Log("Slot Start - ", Commas(toSendCount), " to send");
            }

            inTimeslot = true;

            if (toSendCount)
            {
                ted.RegisterForTimedEvent(0);
            }

            txCount = 0;
            txFailList.clear();
        });
        esb.SetCbOnTimeslotEnd([&]{
            if (slotNotify)
            {
                Log("Slot End");
            }

            inTimeslot = false;

            ted.DeRegisterForTimedEvent();

            if (txFailList.size())
            {
                // LogNNL("TxFailList:");
                // for (auto txNum : txFailList)
                // {
                //     LogNNL(" ", txNum);
                // }
                // LogNL();
            }
        });
        esb.SetCbOnRx([&](Message msg){
            Data *d = (Data *)msg.GetBuf();

            if (rxNotify)
            {
                LogNL();
                Log("RX: ", msg.GetBufSize(), " b, rssi ", msg.GetRssi());
                Log("seqNoInt: ", d->seqNo);
                LogNL();
            }

            if (seqNo && d->seqNo > seqNo + 1)
            {
                Log("rx gap ", d->seqNo - seqNo, ": ", Commas(d->seqNo), ", ", Commas(seqNo));
            }
            else if (seqNo && d->seqNo == seqNo)
            {
                Log("dup ", Commas(d->seqNo));
            }

            seqNo = d->seqNo;
            
            ++rxCount;
        });
        esb.SetCbOnAssert([&]{
            esb.timeline_.ReportNow();
        });

        Shell::AddCommand("esb.max", [&](vector<string> argList){
            if (argList[0] == "on")
            {
                Log("esb.max on");
                esb.EnableTimeslotExtensions();
            }
            else
            {
                Log("esb.max off");
                esb.DisableTimeslotExtensions();
            }
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.notify", [&](vector<string> argList){
            slotNotify = !slotNotify;
            Log("slotNotify now ", slotNotify ? "on" : "off");
        }, { .argCount = 0, .help = "slot on/off notification toggle" });

        Shell::AddCommand("esb.txnotify", [&](vector<string> argList){
            txNotify = !txNotify;
            Log("txNotify now ", txNotify ? "on" : "off");
        }, { .argCount = 0, .help = "tx sent notification toggle" });

        Shell::AddCommand("esb.rxnotify", [&](vector<string> argList){
            rxNotify = !rxNotify;
            Log("rxNotify now ", rxNotify ? "on" : "off");
        }, { .argCount = 0, .help = "rx sent notification toggle" });

        Shell::AddCommand("esb.start", [&](vector<string> argList){
            Log("esb.starting");
            esb.Start();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("esb.stop", [&](vector<string> argList){
            Log("esb.stopping");
            esb.Stop();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("esb.mode", [&](vector<string> argList){
            if (argList[0] == "rx")
            {
                Log("esb.mode rx");
                esb.SetMode(Esb::Mode::RX);
            }
            else
            {
                Log("esb.mode tx");
                esb.SetMode(Esb::Mode::TX);
            }
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.addr.rx", [&](vector<string> argList){
            uint16_t addr = atoi(argList[0].c_str());
            Log("esb.addr.rx ", addr);
            esb.Stop();
            esb.SetAddrRx(addr);
            esb.Start();
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.addr.tx", [&](vector<string> argList){
            uint16_t addr = atoi(argList[0].c_str());
            Log("esb.addr.tx ", addr);
            esb.Stop();
            esb.SetAddrTx(addr);
            esb.Start();
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.pwr.tx", [&](vector<string> argList){
            uint16_t pct = atoi(argList[0].c_str());
            Log("esb.pwr.tx ", pct);
            esb.Stop();
            esb.SetTxPowerPct(pct);
            esb.Start();
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.phy", [&](vector<string> argList){
            Phy phy = StrToPhy(argList[0]);
            Log("esb.phy ", PhyToStr(phy));
            esb.Stop();
            esb.SetPhy(phy);
            esb.Start();
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.chan", [&](vector<string> argList){
            uint8_t chan = atoi(argList[0].c_str());
            Log("esb.chan ", chan);
            esb.Stop();
            esb.SetChannel(chan);
            esb.Start();
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.send.size", [&](vector<string> argList){
            sendSize = min(max((size_t)atoi(argList[0].c_str()), sizeof(Data)), (size_t)Message::BUF_CAPACITY);
            Log("esb.send.size ", sendSize);
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.send", [&](vector<string> argList){
            toSendCount = atoi(argList[0].c_str());
            toSendCountOrig = toSendCount;
            toSendStartTime = PAL.Micros();
            Log("esb.send ", toSendCount);
            ted.RegisterForTimedEvent(0);
        }, { .argCount = 1, .help = "" });

        Shell::AddCommand("esb.report", [&](vector<string> argList){
            esb.Report();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("esb.stats", [&](vector<string> argList){
            esb.PrintStats();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("esb.align", [&](vector<string> argList){
	// uint8_t length; /**< Length of the packet when not in DPL mode. */
	// uint8_t pipe;   /**< Pipe used for this payload. */
	// int8_t rssi;   /**< RSSI for the received packet. */
	// uint8_t noack;  /**< Flag indicating that this packet will not be
	// 	       *  acknowledged. Flag is ignored when selective auto
	// 	       *  ack is enabled.
	// 	       */
	// uint8_t pid;    /**< PID assigned during communication. */
	// uint8_t data[CONFIG_ESB_MAX_PAYLOAD_LENGTH]; /**< The payload data. */
            
            Log("sizeof(esb_payload) = ", sizeof(esb_payload));

            Log("tx : ", &esb.esbPayloadTx_);
            Log("tx : ", &esb.esbPayloadTx_.data);
            Log("rxf: ", &esb.esbPayloadRxFailSafe_);
            Log("rxf: ", &esb.esbPayloadRxFailSafe_.data);
            esb_payload *p = esb.esbPayloadRxPool_.Borrow();
            Log("rx : ", p);
            Log("rx : ", p->data);
            // bus fault
            // double *dp = (double *)p->data;
            // *dp = 8.8;
            // Log("dp: ", *dp);
            esb.esbPayloadRxPool_.Return(p);

            // so as expected, the buf begins at +5 from struct start, making
            // using that as a blob for building structures in is a bad idea.
            // shift the start of the blob forward until it's aligned.
            

        }, { .argCount = 0, .help = "" });
    }


};


