#include "BleAttDatabase.h"
#include "BleGatt.h"
#include "Log.h"
#include "PAL.h"
#include "Timeline.h"
#include "Utl.h"

#include "btstack.h"

#include <unordered_map>
using namespace std;

#include "StrictMode.h"






//
// list service handle ranges
//
#define ATT_SERVICE_GAP_SERVICE_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_END_HANDLE 0x0003
#define ATT_SERVICE_GAP_SERVICE_01_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_01_END_HANDLE 0x0003
#define ATT_SERVICE_GATT_SERVICE_START_HANDLE 0x0004
#define ATT_SERVICE_GATT_SERVICE_END_HANDLE 0x0006
#define ATT_SERVICE_GATT_SERVICE_01_START_HANDLE 0x0004
#define ATT_SERVICE_GATT_SERVICE_01_END_HANDLE 0x0006
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_START_HANDLE 0x0007
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_END_HANDLE 0x000a
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_01_START_HANDLE 0x0007
#define ATT_SERVICE_ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING_01_END_HANDLE 0x000a

//
// list mapping between characteristics and handles
//
#define ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_GATT_DATABASE_HASH_01_VALUE_HANDLE 0x0006
#define ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE 0x0009
#define ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_CLIENT_CONFIGURATION_HANDLE 0x000a




int le_notification_enabled;
hci_con_handle_t con_handle;




uint16_t current_temp = 42;




/////////////////////////////////////////////////////////////////////
// Read
/////////////////////////////////////////////////////////////////////

// per handle, can be done in parallel
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

static unordered_map<uint16_t, BleCharacteristic &> handleMap_;


static uint64_t timeAtEvm;

uint16_t att_read_callback(hci_con_handle_t  conn,
                           uint16_t          handle,
                           uint16_t          offset,
                           uint8_t          *buf,
                           uint16_t          bufSize)
{
    Log("att_read_callback(conn=", ToHex(conn), ", handle=", ToHex(handle), ", offset=", offset, ", bufSize=", bufSize);

    if (readState_.readyToSend == false)
    {
        if (handle == ATT_READ_RESPONSE_PENDING)
        {
            // cache single handle (only one ATT transaction at a time, so this is safe)

            Log("Got 'last in read batch' notification");

            if (handleMap_.contains(readState_.handle))
            {
                Evm::QueueWork("att_read_callback", []{
                    timeAtEvm = PAL.Micros();

                    readState_.readyToSend = true;

                    {
                        IrqLock lock;

                        readState_.byteList.clear();
                        handleMap_.at(readState_.handle).GetCallbackOnRead()(readState_.byteList);

                        readState_.bytesToRead = (uint16_t)readState_.byteList.size();

                        Log(readState_.bytesToRead, " bytes acquired from read callback");
                    }

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
            Log("will send later");

            // cache connection and handle
            readState_.conn = conn;
            readState_.handle = handle;

            return ATT_READ_RESPONSE_PENDING;
        }
    }
    else
    {
        uint64_t timeDiff = PAL.Micros() - timeAtEvm;

        Log("Sending, ", timeDiff, " us later");

        // what if buf changes size before completion?
        // actually, they say only a single att transaction at a time, so this shouldn't happen
        // return att_read_callback_handle_blob((const uint8_t *)&timeAtEvm, sizeof(timeAtEvm), offset, buf, bufSize);

        uint16_t retVal =
            att_read_callback_handle_blob(readState_.byteList.data(),
                                          (uint16_t)readState_.byteList.size(),
                                          offset,
                                          buf,
                                          bufSize);

        // I know this will get called multiple times, but I just don't think I
        // have to worry about it due to one at a time transactions
        //
        // ok wrong, how do you know it's the last transaction?

        uint16_t bytesLeftAfterThisSend  = readState_.bytesToRead - min(readState_.bytesToRead, (uint16_t)(offset + bufSize));
        uint16_t bytesLeftBeforeThisSend = readState_.bytesToRead - min(offset, readState_.bytesToRead);
        uint16_t bytesThisTime = min(bytesLeftBeforeThisSend, bufSize);

        Log("Sending ", bytesThisTime, " bytes, ", bytesLeftAfterThisSend, " bytes remain");

        if (bytesLeftAfterThisSend == 0)
        {
            Log("Declaring this the end of this send");
            readState_.readyToSend = false;
        }

        return retVal;
    }

    return 0;


    if (handle == ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE){
        return att_read_callback_handle_blob((const uint8_t *)&current_temp, sizeof(current_temp), offset, buf, bufSize);
    }
    return 0;
}





/////////////////////////////////////////////////////////////////////
// Write
/////////////////////////////////////////////////////////////////////


static const uint16_t WRITE_BUF_SIZE = 512;
// static const uint16_t WRITE_BUF_SIZE = 17;
struct WriteState
{
    uint16_t        handle;
    vector<uint8_t> byteList;
    bool            bufferExceeded = false;
};
static WriteState writeState_;

static void ResetWriteState()
{
    writeState_ = WriteState{};
}

int att_write_callback(hci_con_handle_t  conn,
                       uint16_t          handle,
                       uint16_t          txnMode,
                       uint16_t          offset,
                       uint8_t          *buf,
                       uint16_t          bufSize)
{
    int retVal = -1;

    Log("att_write_callback(conn=", ToHex(conn), ", handle=", ToHex(handle), ", txnMode=", txnMode, ", offset=", offset, ", bufSize=", bufSize);
    
    // track whether we have an entire data set to pass along
    bool commitData = false;

    if (txnMode == ATT_TRANSACTION_MODE_NONE)
    {
        // This condition is a write where the total bytes fit completely
        // within the btstacks buffer and can be conveyed in a single
        // callback.
        // if you can store it, great, do it and call back,
        // otherwise indicate failure.

        Log("Write single-shot");

        // cache
        writeState_.handle = handle;
        
        // take in bytes
        if (bufSize <= WRITE_BUF_SIZE)
        {
            Log("  Buffer within range, committing");

            retVal = 0;

            for (uint16_t i = 0; i < bufSize; ++i)
            {
                writeState_.byteList.push_back(buf[i]);
            }

            commitData = true;
        }
        else
        {
            Log("  Buffer too large, cancelling");
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_ACTIVE)
    {
        // portion of a multi-part write consisting of more than one buffer
        // of data to be delivered.
        // if you run out of capacity to cache before the entire packet is
        // stored, you're going to want to indicate failure to capture,
        // which will result in a cancel of the entire transaction.

        Log("Write multi-shot");
        
        // cache
        writeState_.handle = handle;

        if (writeState_.byteList.size() + bufSize <= WRITE_BUF_SIZE)
        {
            retVal = 0;

            Log("  Chunk fits, progressing");

            for (uint16_t i = 0; i < bufSize; ++i)
            {
                writeState_.byteList.push_back(buf[i]);
            }
        }
        else
        {
            Log("  Chunk doesn't fit, remembering as bad");
            writeState_.bufferExceeded = true;
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_VALIDATE)
    {
        // btstacks is asking if everything worked out ok.
        // if it did, indicate so.  progress to EXECUTE.
        // if not, indicate so.  progress to CANCEL.

        Log("Write validation");

        if (writeState_.bufferExceeded == false)
        {
            retVal = 0;
            Log("  Good, progressing");
        }
        else
        {
            Log("  Bad, cancelling");
        }
    }
    else if (txnMode == ATT_TRANSACTION_MODE_EXECUTE)
    {
        // handle expected to be 0x0000
        Log("Write executing");
        
        retVal = 0;
        commitData = true;
        Log("  Buffer ok, committing");
    }
    else if (txnMode == ATT_TRANSACTION_MODE_CANCEL)
    {
        // handle expected to be 0x0000

        Log("Write Cancel, resetting");
        ResetWriteState();
    }

    if (commitData)
    {
        if (handleMap_.contains(writeState_.handle))
        {
            Evm::QueueWork("att_read_callback", []{
                IrqLock lock;

                handleMap_.at(writeState_.handle).GetCallbackOnWrite()(writeState_.byteList);

                Log(writeState_.byteList.size(), " bytes written to write callback");

                writeState_.byteList.clear();
            });
        }
        else
        {
            Log("ERR: No write handler for handle ", ToHex(handle));

            writeState_.byteList.clear();
        }
    }

    return retVal;


    
    if (handle != ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    le_notification_enabled = little_endian_read_16(buf, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    con_handle = conn;
    if (le_notification_enabled) {
        att_server_request_can_send_now_event(con_handle);
    }
    return 0;
}


/*
#define ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS            0x90 
#define ATT_HANDLE_VALUE_INDICATION_TIMEOUT                0x91
#define ATT_HANDLE_VALUE_INDICATION_DISCONNECT             0x92

#define ATT_PROPERTY_BROADCAST           0x01
#define ATT_PROPERTY_READ                0x02
#define ATT_PROPERTY_WRITE_WITHOUT_RESPONSE 0x04
#define ATT_PROPERTY_WRITE               0x08
#define ATT_PROPERTY_NOTIFY              0x10
#define ATT_PROPERTY_INDICATE            0x20
#define ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE 0x40
#define ATT_PROPERTY_EXTENDED_PROPERTIES 0x80
*/

// HCI, GAP, and general BTstack events
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
            hci_con_handle_t handle = att_event_connected_get_handle(packet);
            uint16_t mtu = att_server_get_mtu(handle);

            Log("ATT_EVENT_CONNECTED");
            Log("- ", ToHex(handle), " at mtu ", mtu);
            LogNL();

            

            // context = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
            // if (!context) break;
            // context->counter = A;
            // context->connection_handle = att_event_connected_get_handle(packet);
            // context->test_data_len = btstack_min(att_server_get_mtu(context->connection_handle) - 3, sizeof(context->test_data));
            // printf("%c: ATT connected, handle 0x%04x, test data len %u\n", context->name, context->connection_handle, context->test_data_len);
        }
        else if (eventType == ATT_EVENT_MTU_EXCHANGE_COMPLETE)
        {
            hci_con_handle_t handle = att_event_mtu_exchange_complete_get_handle(packet);
            uint16_t mtu = att_event_mtu_exchange_complete_get_MTU(packet);

            Log("ATT_EVENT_MTU_EXCHANGE_COMPLETE - ", handle, " at mtu ", mtu);

            // mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            // context = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            // if (!context) break;
            // context->test_data_len = btstack_min(mtu - 3, sizeof(context->test_data));
            // printf("%c: ATT MTU = %u => use test data of len %u\n", context->name, mtu, context->test_data_len);
        }
        else if (eventType == ATT_EVENT_CAN_SEND_NOW)
        {
            Log("ATT_EVENT_CAN_SEND_NOW");

            // streamer();
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE, (uint8_t*)&current_temp, sizeof(current_temp));
        }
        else if (eventType == ATT_EVENT_DISCONNECTED)
        {
            hci_con_handle_t handle = att_event_disconnected_get_handle(packet);

            Log("ATT_EVENT_DISCONNECTED - ", handle);

            // context = connection_for_conn_handle(att_event_disconnected_get_handle(packet));
            // if (!context) break;
            // // free connection
            // printf("%c: ATT disconnected, handle 0x%04x\n", context->name, context->connection_handle);                    
            // context->le_notification_enabled = 0;
            // context->connection_handle = HCI_CON_HANDLE_INVALID;
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
// Interface
/////////////////////////////////////////////////////////////////////

static vector<uint8_t> attDbByteList_;

void BleGatt::Init(string name, vector<BlePeripheral> &periphList)
{
    Timeline::Global().Event("BleGatt::Init");

    // Generate database
    BleAttDatabase attDb(name);

    for (int count = 1; auto &p : periphList)
    {
        string fqn = p.GetName();
        ++count;
        Log("Registering Peripheral ", fqn);

        for (auto &[svcName, svc] : p.GetServiceList())
        {
            string fqn2 = fqn + "." + svc.GetName() + "(" + svc.GetUuid() + ")";
            Log("  Registering Service ", fqn2);

            attDb.AddPrimaryService(svc.GetUuid());

            for (auto &[ctcName, ctc] : svc.GetCtcList())
            {
                string fqn3 = fqn2 + "." + ctc.GetName() + "(" + ctc.GetUuid() + ")";

                vector<uint16_t> handleList =
                    attDb.AddCharacteristic(ctc.GetUuid(),
                                            ctc.GetProperties());
                
                Log("    Registering CTC ", fqn3, ", handles: ", handleList);

                // associate handles with the characteristic
                for (auto handle : handleList)
                {
                    handleMap_.emplace(handle, ctc);
                }
            }
        }
    }

    // capture raw database structure
    attDbByteList_ = attDb.GetDatabaseData();

    // allocate space for receive buffer

    writeState_.byteList.reserve(WRITE_BUF_SIZE);
}


void BleGatt::OnReady()
{
    Timeline::Global().Event("BleGatt::OnReady");

    // register for ATT event
    att_server_register_packet_handler(PacketHandlerATT);

    // https://bluekitchen-gmbh.com/btstack/#appendix/att_server/
    att_server_init(attDbByteList_.data(), att_read_callback, att_write_callback);



    

    // static TimedEventHandlerDelegate ted;
    // ted.SetCallback([]{
    //     if (le_notification_enabled) {
    //         att_server_request_can_send_now_event(con_handle);
    //     }
    // });
    // ted.RegisterForTimedEventInterval(5000, 0);

    Log("BLE Services Started");
}