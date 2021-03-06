#include <nRF5x_BLE_API.h>

#define DEBUG

#ifdef DEBUG
#define sprintln Serial.println
#define sprint Serial.print
#define swrite Serial.write
#else
#define sprintln(fmt, ...) ;
#define sprint(fmt, ...) ;
#define swrite(fmt, ...) ;
#endif

#define TXRX_BUF_LEN 20
#define DEVICE_NAME       "Sukoyaka Next"

BLE ble;

// Timer
Ticker tickerLed;
int led_cnt;
int led_num = 1;

// The Nordic UART Service
static const uint8_t service1_uuid[]        = {0x71, 0x3D, 0, 0, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
static const uint8_t service1_chars1_uuid[] = {0x71, 0x3D, 0, 2, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
static const uint8_t uart_base_uuid_rev[]   = {0x1E, 0x94, 0x8D, 0xF1, 0x48, 0x31, 0x94, 0xBA, 0x75, 0x4C, 0x3E, 0x50, 0, 0, 0x3D, 0x71};
 
uint8_t chars1_value[TXRX_BUF_LEN] = {0,};

GattCharacteristic characteristic1(service1_chars1_uuid, chars1_value, 0, TXRX_BUF_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE );
//通知
GattCharacteristic *uartChars[] = {&characteristic1, /*&characteristic2, &characteristic3*/};
GattService uartService(service1_uuid, uartChars, sizeof(uartChars) / sizeof(GattCharacteristic *));

//接続がされたときの処理
void connectionCallBack( const Gap::ConnectionCallbackParams_t *params ) {
  sprintln("Conect");
  if(params->role == Gap::PERIPHERAL) {
    sprintln("Peripheral ");
  }
}

//切断
void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  sprint("Disconnected hande : ");
  sprintln(params->handle);
  sprint("Disconnected reason : ");
  sprintln(params->reason);
  sprintln("Restart advertising ");
  ble.startAdvertising();
}

//データの読み込み
void writtenHandle(const GattWriteCallbackParams *Handler) {
  digitalWrite(13, LOW);
  sprint("writtenHandle\t");
  sprint(Handler->handle, HEX);
  sprint("\t");
  sprintln(characteristic1.getValueAttribute().getHandle(), HEX);
  uint8_t buf[TXRX_BUF_LEN];
  uint16_t bytesRead = TXRX_BUF_LEN;
  uint16_t index;

  if (Handler->handle == characteristic1.getValueAttribute().getHandle()) {
    ble.readCharacteristicValue(characteristic1.getValueAttribute().getHandle(), buf, &bytesRead);
    sprintln(bytesRead);
    for(byte index=0; index<bytesRead; index++) {
      sprint(buf[index], HEX);
      sprint(" ");
    }
    sprintln("");
    led_num = bytesRead;
  }
  digitalWrite(13, HIGH);
}

void task_handle_led(void) {
  ++led_cnt;
  if (led_cnt > led_num * 2 + 3) {
    led_cnt = 0;
  }
  if (led_cnt & 0x1 && led_cnt < led_num * 2) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }
}


void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);

   // タイマー
  tickerLed.attach_us(task_handle_led, 200000);

  //BLE設定
  ble.init();
  ble.onConnection(connectionCallBack);
  ble.onDisconnection(disconnectionCallBack);
  ble.onDataWritten(writtenHandle);
      
  // setup adv_data and srp_data
  ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,(const uint8_t *)"TXRX", sizeof("TXRX") - 1);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,(const uint8_t *)uart_base_uuid_rev, sizeof(uart_base_uuid_rev));
  ble.accumulateScanResponse(GapAdvertisingData::COMPLETE_LOCAL_NAME,(const uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME) - 1);

  // set adv_type
  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);    
  // add service
  ble.addService(uartService);
  // set device name
  ble.setDeviceName((const uint8_t *)DEVICE_NAME);
  // set tx power,valid values are -40, -20, -16, -12, -8, -4, 0, 4
  ble.setTxPower(4);
  // set adv_interval, 100ms in multiples of 0.625ms.
  ble.setAdvertisingInterval(160);
  // set adv_timeout, in seconds
  ble.setAdvertisingTimeout(0);
  // start advertising
  ble.startAdvertising();

  sprintln("Advertising Start!"); 
}

void loop() {   
  ble.waitForEvent();
}
