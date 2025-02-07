#include "BleAttDatabase.h"
#include "BleGatt.h"
#include "Evm.h"
#include "Log.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include "btstack.h"

#include <unordered_map>
using namespace std;

#include "StrictMode.h"


/*

This whole implementation is assuming a single connection.

Probably should abstract to keeping state up to 
MAX_NR_HCI_CONNECTIONS (I think?) connections.

Keep it simple for now.

*/


/////////////////////////////////////////////////////////////////////
// State
/////////////////////////////////////////////////////////////////////

static unordered_map<uint16_t, BleCharacteristic &> handleReadWriteMap_;
static unordered_map<uint16_t, BleCharacteristic &> handleNotifyMap_;

static hci_con_handle_t conn_;
static bool connected_ = false;

// for notify
static uint16_t handle_;

// for read - per connection
struct ReadState
{
    bool             readyToSend = false;
    hci_con_handle_t conn;
    uint16_t         handle;
    uint64_t         timeAtEvm;
    vector<uint8_t>  byteList;
    uint16_t         bytesToRead;
};

static ReadState readState_;

static void ResetReadState()
{
    readState_ = ReadState{};
}

// for write - per connection
static const uint16_t WRITE_BUF_SIZE = 256;
struct WriteState
{
    uint16_t        handle;
    vector<uint8_t> byteList;
    bool            bufferExceeded = false;
    uint16_t        bytesAccumulated = 0;
};
static WriteState writeState_;

static void ResetWriteState()
{
    writeState_ = WriteState{};
}

static void ResetWriteStateButOnlyTrashByesAccumulated()
{
    writeState_.handle = 0;
    writeState_.byteList.erase(writeState_.byteList.end() - writeState_.bytesAccumulated,
                               writeState_.byteList.end());
    writeState_.bufferExceeded = false;
    writeState_.bytesAccumulated = 0;

}

// general
static void InitState()
{
    writeState_.byteList.reserve(WRITE_BUF_SIZE);
}

// connection
static void OnConnect(hci_con_handle_t conn)
{
    conn_      = conn;
    connected_ = true;

    ResetReadState();
    ResetWriteState();
}

static void OnDisconnect(hci_con_handle_t conn)
{
    connected_ = false;
}


/////////////////////////////////////////////////////////////////////
// Read
/////////////////////////////////////////////////////////////////////

static
uint16_t att_read_callback_read(hci_con_handle_t  conn,
                                uint16_t          handle,
                                uint16_t          offset,
                                uint8_t          *buf,
                                uint16_t          bufSize)
{
    if (readState_.readyToSend == false)
    {
        if (handle == ATT_READ_RESPONSE_PENDING)
        {
            if (handleReadWriteMap_.contains(readState_.handle))
            {
                Evm::QueueWork("QueueWork att_read_callback", []{
                    readState_.timeAtEvm = PAL.Micros();

                    readState_.readyToSend = true;

                    // do I need to lock any part of this?
                    readState_.byteList.clear();
                    handleReadWriteMap_.at(readState_.handle).GetCallbackOnRead()(readState_.byteList);

                    readState_.bytesToRead = (uint16_t)readState_.byteList.size();

                    // Log(readState_.bytesToRead, " bytes acquired from read callback");

                    att_server_response_ready(readState_.conn);
                });
            }
            else
            {
                Log("ERR: No read handler for handle ", ToHex(handle));
            }

            return ATT_READ_RESPONSE_PENDING;
        }
        else
        {
            // Log("will send later");

            // cache connection and handle
            readState_.conn = conn;
            readState_.handle = handle;

            return ATT_READ_RESPONSE_PENDING;
        }
    }
    else
    {
        // what if buf changes size before completion?
        // actually, they say only a single att transaction at a time, so this shouldn't happen

        // Log("handling: ", (uint32_t)readState_.byteList.data(), ": ", readState_.byteList.size());

        // protect against .data() returning nullptr when size() == 0
        // (which would be possible if no storage allocated yet and
        //  handler didn't return any data)
        uint8_t byte = '\0';
        uint8_t *p = readState_.byteList.data();
        if (p == nullptr)
        {
            p = &byte;
        }

        uint16_t retVal =
            att_read_callback_handle_blob(p,
                                          (uint16_t)readState_.byteList.size(),
                                          offset,
                                          buf,
                                          bufSize);

        // before each real send the btstack will trigger this event to measure
        // the data left to send.  no point in logging that.
        if (bufSize)
        {
            uint16_t bytesLeftAfterThisSend  = readState_.bytesToRead - min(readState_.bytesToRead, (uint16_t)(offset + bufSize));
            // uint16_t bytesLeftBeforeThisSend = readState_.bytesToRead - min(offset, readState_.bytesToRead);
            // uint16_t bytesThisTime = min(bytesLeftBeforeThisSend, bufSize);
            // Log("Sending ", bytesThisTime, " bytes, ", bytesLeftAfterThisSend, " bytes remain");

            if (bytesLeftAfterThisSend == 0)
            {
                // Log("Declaring this the end of this send");
                readState_.readyToSend = false;
            }
        }

        return retVal;
    }
}

static
uint16_t att_read_callback_notify(hci_con_handle_t  conn,
                                  uint16_t          handle,
                                  uint16_t          offset,
                                  uint8_t          *buf,
                                  uint16_t          bufSize)
{
    uint16_t value = handleNotifyMap_.at(handle).GetIsSubscribed();

    uint16_t retVal =
        att_read_callback_handle_blob((uint8_t *)&value,
                                       sizeof(value),
                                       offset,
                                       buf,
                                       bufSize);

    return retVal;
}

static
uint16_t att_read_callback(hci_con_handle_t  conn,
                           uint16_t          handle,
                           uint16_t          offset,
                           uint8_t          *buf,
                           uint16_t          bufSize)
{
    // Log("att_read_callback(conn=", ToHex(conn), ", handle=", ToHex(handle), ", offset=", offset, ", bufSize=", bufSize);

    uint16_t retVal = 1;

    if (handleNotifyMap_.contains(handle))
    {
        retVal = att_read_callback_notify(conn, handle, offset, buf, bufSize);
    }
    else
    {
        retVal = att_read_callback_read(conn, handle, offset, buf, bufSize);
    }

    return retVal;
}


/////////////////////////////////////////////////////////////////////
// Write
/////////////////////////////////////////////////////////////////////

/*

There are two modes that writes can be performed in:
- Single-shot
- Chunked ("prepared writes")

The transaction type indicates.

For Single-shot, you get a transaction type of ATT_TRANSACTION_MODE_NONE.

For chunked you:
- start with ATT_TRANSACTION_MODE_ACTIVE
  - repeatedly get those until all data is delivered
- then ATT_TRANSACTION_MODE_VALIDATE
- then ATT_TRANSACTION_MODE_EXECUTE

If, during chunked, you return an error (non-zero), there will
be no more ATT_TRANSACTION_MODE_ACTIVE, and you will go next
to ATT_TRANSACTION_MODE_CANCEL.

You can also error out on ATT_TRANSACTION_MODE_VALIDATE
to get to ATT_TRANSACTION_MODE_CANCEL (I think)



#define ATT_TRANSACTION_MODE_NONE      0x0
#define ATT_TRANSACTION_MODE_ACTIVE    0x1
#define ATT_TRANSACTION_MODE_EXECUTE   0x2
#define ATT_TRANSACTION_MODE_CANCEL    0x3
#define ATT_TRANSACTION_MODE_VALIDATE  0x4


What I see happen with various writes
- small packet (less than 22 bytes): ATT_TRANSACTION_MODE_NONE
- large packet (23+ bytes):
  - handle=0x000D, txnMode=1, offset=0, bufSize=18  // ACTIVE (with handle)
  - handle=0x000D, txnMode=1, offset=18, bufSize=5  // ACTIVE (with handle)
  - handle=0x0000, txnMode=4, offset=0, bufSize=0   // VALIDATE (with no handle)
  - handle=0x0000, txnMode=2, offset=0, bufSize=0   // EXECUTE  (with no handle)

*/

static
int att_write_callback_write(hci_con_handle_t  conn,
                             uint16_t          handle,
                             uint16_t          txnMode,
                             uint16_t          offset,
                             uint8_t          *buf,
                             uint16_t          bufSize)
{
    int retVal = -1;

    // track whether we have an entire data set to pass along
    bool commitData = false;

    if (txnMode == ATT_TRANSACTION_MODE_NONE)
    {
        // This condition is a write where the total bytes fit completely
        // within the btstacks buffer and can be conveyed in a single
        // callback.
        // if you can store it, great, do it and call back,
        // otherwise indicate failure.

        // Log("Write single-shot");

        // cache
        writeState_.handle = handle;
        
        // take in bytes
        if (writeState_.byteList.size() + bufSize <= WRITE_BUF_SIZE)
        {
            // Log("  Buffer within range, committing");

            retVal = 0;
            writeState_.bytesAccumulated = bufSize;

            for (uint16_t i = 0; i < bufSize; ++i)
            {
                writeState_.byteList.push_back(buf[i]);
            }

            commitData = true;
        }
        else
        {
            // Log("  Buffer too large, cancelling");
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_ACTIVE)
    {
        // portion of a multi-part write consisting of more than one buffer
        // of data to be delivered.
        // if you run out of capacity to cache before the entire packet is
        // stored, you're going to want to indicate failure to capture,
        // which will result in a cancel of the entire transaction.

        // Log("Write multi-shot");
        
        // cache
        writeState_.handle = handle;

        if (writeState_.byteList.size() + bufSize <= WRITE_BUF_SIZE)
        {
            retVal = 0;
            writeState_.bytesAccumulated += bufSize;

            // Log("  Chunk fits, progressing");

            for (uint16_t i = 0; i < bufSize; ++i)
            {
                writeState_.byteList.push_back(buf[i]);
            }
        }
        else
        {
            // Log("  Chunk doesn't fit, remembering as bad");
            writeState_.bufferExceeded = true;
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_VALIDATE)
    {
        // btstacks is asking if everything worked out ok.
        // if it did, indicate so.  progress to EXECUTE.
        // if not, indicate so.  progress to CANCEL.

        // Log("Write validation");

        if (writeState_.bufferExceeded == false)
        {
            retVal = 0;
            // Log("  Good, progressing");
        }
        else
        {
            // Log("  Bad, cancelling");
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_EXECUTE)
    {
        // handle expected to be 0x0000
        // Log("Write executing");
        
        retVal = 0;
        commitData = true;
        // Log("  Buffer ok, committing");
    }
    else if (txnMode == ATT_TRANSACTION_MODE_CANCEL)
    {
        // handle expected to be 0x0000

        // Log("Write Cancel, resetting");
        ResetWriteStateButOnlyTrashByesAccumulated();
    }

    if (commitData)
    {
        if (handleReadWriteMap_.contains(writeState_.handle))
        {
            // keep a non-global copy for labda to capture by value
            uint16_t bytesAccumulated = writeState_.bytesAccumulated;

            Evm::QueueWork("QueueWork att_write_callback", [=]{
                vector<uint8_t> byteList;

                {
                    IrqLock lock;

                    // copy the bytes that came in this write
                    byteList = {writeState_.byteList.begin(),
                                writeState_.byteList.begin() + bytesAccumulated};

                    // remove those bytes from write state
                    writeState_.byteList.erase(writeState_.byteList.begin(),
                                               writeState_.byteList.begin() + bytesAccumulated);
                }

                handleReadWriteMap_.at(writeState_.handle).GetCallbackOnWrite()(byteList);

                // Log(writeState_.bytesAccumulated, " bytes written to write callback, queue size: ", writeState_.byteList.size());
            });
        }
        else
        {
            Log("ERR: No write handler for handle ", ToHex(handle));
        }
    }

    return retVal;
}

static
int att_write_callback_notify(hci_con_handle_t  conn,
                              uint16_t          handle,
                              uint16_t          txnMode,
                              uint16_t          offset,
                              uint8_t          *buf,
                              uint16_t          bufSize)
{
    int retVal = -1;

    if (bufSize >= 2)
    {
        retVal = 0;

        bool enabled = (bool)little_endian_read_16(buf, 0);

        // Log("Notify for ", ToHex(handle), ": ", enabled, ", bufSize ", bufSize);

        Evm::QueueWork("QueueWork att_write_callback_notify", [=]{
            handleNotifyMap_.at(handle).GetCallbackOnSubscribe()(enabled);
        });
    }

    return retVal;
}

static
int att_write_callback(hci_con_handle_t  conn,
                       uint16_t          handle,
                       uint16_t          txnMode,
                       uint16_t          offset,
                       uint8_t          *buf,
                       uint16_t          bufSize)
{
    // Log("att_write_callback(conn=", ToHex(conn), ", handle=", ToHex(handle), ", txnMode=", txnMode, ", offset=", offset, ", bufSize=", bufSize);
    
    int retVal = -1;

    if (handleNotifyMap_.contains(handle))
    {
        retVal = att_write_callback_notify(conn, handle, txnMode, offset, buf, bufSize);
    }
    else
    {
        retVal = att_write_callback_write(conn, handle, txnMode, offset, buf, bufSize);
    }

    return retVal;
}


/////////////////////////////////////////////////////////////////////
// Notify
/////////////////////////////////////////////////////////////////////

// Only support a single ctc notify for the time being.
// Otherwise have to maintain a list of them or something
// and I just don't have that use case right now.
//
// This implementation is just getting the mechanisms
// from btstack working.  Can expand later after that is sorted.
static void TriggerNotify(uint16_t handle)
{
    handle_ = handle;

    // BleCharacteristic &ctc = handleReadWriteMap_.at(handle_);
    // Log("Triggering notify for ", ctc.GetName(), " if connected (", connected_, ")");

    if (connected_)
    {
        att_server_request_can_send_now_event(conn_);
    }
}

static void DoNotify()
{
    Evm::QueueWork("QueueWork BleGatt::Notify", []{
        if (connected_)
        {
            BleCharacteristic &ctc = handleReadWriteMap_.at(handle_);

            vector<uint8_t> byteList;

            ctc.GetCallbackOnRead()(byteList);

            // Log("Doing notify for ", ctc.GetName(), ", ", byteList.size(), " bytes returned");

            uint8_t retVal = att_server_notify(conn_, handle_, byteList.data(), (uint16_t)byteList.size());

            if (retVal == 0)
            {
                // Log("  Success");
            }
            else
            {
                // Log("  Error: ", retVal);
            }
        }
    });
}


/////////////////////////////////////////////////////////////////////
// Packet handling
/////////////////////////////////////////////////////////////////////

static void PacketHandlerATT(uint8_t   packet_type,
                             uint16_t  channel,
                             uint8_t  *packet,
                             uint16_t  size)
{
    Log("ATT: type: ", packet_type, ", channel: ", channel, ", size: ", size);

    if (packet_type == HCI_EVENT_PACKET)
    {
        uint8_t eventType = hci_event_packet_get_type(packet);

        if (eventType == ATT_EVENT_CONNECTED)
        {
            hci_con_handle_t conn = att_event_connected_get_handle(packet);

            OnConnect(conn);

            uint16_t mtu = att_server_get_mtu(conn);

            Log("ATT_EVENT_CONNECTED");
            Log("- ", ToHex(conn), " at mtu ", mtu);
            LogNL();
        }
        else if (eventType == ATT_EVENT_MTU_EXCHANGE_COMPLETE)
        {
            hci_con_handle_t conn = att_event_mtu_exchange_complete_get_handle(packet);
            uint16_t mtu = att_event_mtu_exchange_complete_get_MTU(packet);

            Log("ATT_EVENT_MTU_EXCHANGE_COMPLETE - ", conn, " at mtu ", mtu);
        }
        else if (eventType == ATT_EVENT_CAN_SEND_NOW)
        {
            // Log("ATT_EVENT_CAN_SEND_NOW");

            DoNotify();
        }
        else if (eventType == ATT_EVENT_DISCONNECTED)
        {
            hci_con_handle_t conn = att_event_disconnected_get_handle(packet);
            
            OnDisconnect(conn);

            Log("ATT_EVENT_DISCONNECTED - ", conn);
        }
        else if (eventType == HCI_EVENT_LE_META)
        {
            uint8_t code = hci_event_le_meta_get_subevent_code(packet);
            // Log("ATT Handler - HCI_EVENT_LE_META - ", ToHex(code));

            if (code == HCI_SUBEVENT_LE_CONNECTION_COMPLETE)
            {
                Log("ATT HCI_SUBEVENT_LE_CONNECTION_COMPLETE");
                LogNL();
            }
            else
            {
                Log("------- ATT default code ", ToHex(code));
            }
        }
        else
        {
            Log("ATT: default event ", ToHex(eventType));
        }
    }
    else
    {
        Log("ATT: Not HCI_EVENT_PACKET!!!!!");
    }
}


/////////////////////////////////////////////////////////////////////
// Class State
/////////////////////////////////////////////////////////////////////

// keep full lifecycle ownership of the db bytes
static vector<uint8_t> attDbByteList_;

// retain full lifecycle ownership of the set of services that
// callbacks are hooked into
static vector<BleService> svcList_;

// use this to know whether HCI ready
static bool ready_ = false;


/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

// can be called at any point, if HCI not ready, or already running in
// another configuration, this takes care of it.
void BleGatt::Init(string name, vector<BleService> &svcList)
{
    Timeline::Global().Event("BleGatt::Init");

    // services could be running already, and those callbacks reach
    // into the service list.  have to atomically swap out.
    IrqLock lock;

    // take full lifecycle ownership of services
    svcList_ = svcList;

    // Generate database
    BleAttDatabase attDb(name);

    // Wipe any state from a prior run
    handleReadWriteMap_.clear();
    handleNotifyMap_.clear();

    Log("GATT: Registering ", svcList_.size(), " services");

    // Build and associate services
    for (auto &svc : svcList_)
    {
        string fqn = svc.GetName() + "(" + svc.GetUuid() + ")";
        Log("  Registering Service ", fqn);

        attDb.AddPrimaryService(svc.GetUuid());

        for (auto &[ctcName, ctc] : svc.GetCtcList())
        {
            string fqn2 = fqn + "." + ctc.GetName() + "(" + ctc.GetUuid() + ")";

            vector<uint16_t> handleList =
                attDb.AddCharacteristic(ctc.GetUuid(),
                                        ctc.GetProperties());
            
            // associate handles with the characteristic
            LogNNL("    Registering CTC ", fqn2, " - handles: ");
            if (handleList.size() >= 2)
            {
                uint16_t handle = handleList[1];
                handleReadWriteMap_.emplace(handle, ctc);
                LogNNL(ToHex(handle), " rw");
            }

            if (handleList.size() >= 3)
            {
                uint16_t handle = handleList[2];
                handleNotifyMap_.emplace(handle, ctc);
                LogNNL(", ", ToHex(handle), " notify");

                ctc.SetCallbackTriggerNotify([=]{
                    TriggerNotify(handleList[1]);
                });
            }
            LogNL();
        }
    }

    // capture raw database structure
    attDbByteList_ = attDb.GetDatabaseData();

    // ensure runtime containers set up
    InitState();

    LogNL();

    // if OnReady already ran, run it again
    if (ready_)
    {
        OnReady();
    }
}

void BleGatt::OnReady()
{
    Timeline::Global().Event("BleGatt::OnReady");

    ready_ = true;

    static bool doOnce = true;

    if (doOnce)
    {
        if (attDbByteList_.size() != 0)
        {
            doOnce = false;

            // register for ATT event
            att_server_register_packet_handler(PacketHandlerATT);

            // https://bluekitchen-gmbh.com/btstack/#appendix/att_server/
            att_server_init(attDbByteList_.data(), att_read_callback, att_write_callback);

            Log("BLE GATT Started");
        }
    }
    else
    {
        att_set_db(attDbByteList_.data());

        Log("BLE GATT Re-Started");
    }
}



/*

Looks like att_server_init() is a convenience function.  It:
- sets handler pointers
- calls att_set_db()
  - just stores a pointer

att_server_deinit()
- just unsets handler pointers
- doesn't do anything about the database being set
  - maybe things even just keep working but app never knows
    because no callbacks?
    - like, connection would still work?

so ok to call att_server_init() the first time
  but subsequently, just call att_set_db()?

*/
void BleGatt::SetupShell()
{
    Shell::AddCommand("ble.gatt.init", [](vector<string> argList){
        static bool doOnce = true;

        handleReadWriteMap_.clear();
        handleNotifyMap_.clear();
        // clean up any state?
        // don't disconnect?
            // just make sure equal services keep same handle?
                // does it even matter?
                    // might not for nordic app, maybe not everything (web, etc) is as smart?

        if (doOnce)
        {
            doOnce = false;

            static vector<BleService> svcList;

            static auto &s = svcList.emplace_back("SERIAL1.1", "0xAAAA");
            static auto &c1 = s.GetOrMakeCharacteristic("SERIAL", "0xAAAA", "READ | NOTIFY | WRITE_WITHOUT_RESPONSE | DYNAMIC");

            c1.SetCallbackOnRead([](vector<uint8_t> &byteList){
                Log("Test Read Callback");
            });
            c1.SetCallbackOnSubscribe([](bool enabled){
                Log("Test C1 Subscribe Callback - ", enabled);
            });

            static auto &s2 = svcList.emplace_back("SERIAL1.2", "0xAAAA");
            s2.GetOrMakeCharacteristic("SERIAL", "0xAAAA", "READ | NOTIFY | WRITE_WITHOUT_RESPONSE | DYNAMIC");

            // test what happens when a service vanishes
            static auto &s3 = svcList.emplace_back("SERIAL1.3", "0xAAAA");
            static auto &c3 = s3.GetOrMakeCharacteristic("SERIAL", "0xAAAA", "READ | NOTIFY | WRITE_WITHOUT_RESPONSE | DYNAMIC");

            c3.SetCallbackOnRead([](vector<uint8_t> &byteList){
                Log("Test C3 Read Callback");
            });

            Init("test", svcList);
        }
        else
        {
            static vector<BleService> svcList;

            static auto &s = svcList.emplace_back("SERIAL2.1", "0xBBBB");
            static auto &c1 = s.GetOrMakeCharacteristic("SERIAL", "0xBBBB", "READ | NOTIFY | WRITE_WITHOUT_RESPONSE | DYNAMIC");

            c1.SetCallbackOnRead([](vector<uint8_t> &byteList){
                Log("Test2 C1 Read Callback");
            });
            c1.SetCallbackOnSubscribe([](bool enabled){
                Log("Test2 C1 Subscribe Callback - ", enabled);
            });

            static auto &s2 = svcList.emplace_back("SERIAL2.2", "0xCCCC");
            static auto &c2 = s2.GetOrMakeCharacteristic("SERIAL", "0xCCCC", "READ | NOTIFY | WRITE_WITHOUT_RESPONSE | DYNAMIC");

            c2.SetCallbackOnRead([](vector<uint8_t> &byteList){
                Log("Test2 C2 Read Callback");
            });

            Init("test2", svcList);
        }
    }, { .argCount = 0, .help = "Init" });

    Shell::AddCommand("ble.gatt.deinit", [](vector<string> argList){
    }, { .argCount = 0, .help = "DeInit" });
}