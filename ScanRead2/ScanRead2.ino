#include <nRF5x_BLE_API.h>

#define DEBUG

#ifdef DEBUG
#define sprintln Serial.println
#define sprint Serial.print
#else
#define sprintln(fmt, ...) ;
#define sprint(fmt, ...) ;
#endif

static uint8_t CHECK_POINT_ID = 0x11;

BLE ble;

// スキャンで見つかったキャラクタリスティックを保存
static DiscoveredCharacteristic Characteristic_values;

// コールバック関数
static void scanCallBack(const Gap::AdvertisementCallbackParams_t *params);
static void discoveredCharacteristicCallBack(const DiscoveredCharacteristic *chars);
static void onDataWriteCallBack(const GattWriteCallbackParams *params);
static void onDataReadCallBack(const GattReadCallbackParams *params);


//アドバタイジングのデータを解析してデバイス名を返す
uint32_t ble_advdata_parser(uint8_t type, uint8_t advdata_len, uint8_t *p_advdata, uint8_t *len, uint8_t *p_field_data) {
  uint8_t index=0;
  uint8_t field_length, field_type;

  while(index<advdata_len) {
    field_length = p_advdata[index];
    field_type   = p_advdata[index+1];
    sprint("field_type : ");
    sprint(field_type, HEX);
    sprint("   type : ");
    sprint(type, HEX);
    uint8_t adv_name[31];
    memset(adv_name, '\0', sizeof adv_name);
    memcpy(adv_name, &p_advdata[index+2], (field_length-1));
    sprint("  name : ");
    sprintln((const char*)adv_name);
    if(field_type == type) {
      memcpy(p_field_data, &p_advdata[index+2], (field_length-1));
      *len = field_length - 1;
      return NRF_SUCCESS;
    }
    index += field_length + 1;
  }
  return NRF_ERROR_NOT_FOUND;
}


// スキャンのコールバック処理
// ショートネームが一致したら接続
static void scanCallBack(const Gap::AdvertisementCallbackParams_t *params) {
  sprintln("* scan call back");
  uint8_t len;
  uint8_t adv_name[31];   // アドバタイジングパケットの最大は31バイトっぽいので、ショートネーム以外でもこのままでいい
  memset(adv_name, '\0', sizeof adv_name);
  if( NRF_SUCCESS == ble_advdata_parser(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, params->advertisingDataLen, (uint8_t *)params->advertisingData, &len, adv_name) ) {
    sprint("Device name len : ");
    sprintln(len, DEC);
    sprint("Device name is : ");
    sprintln((const char*)adv_name);

    if( memcmp("nut", adv_name, 3) == 0x00 ) {          // 「nut」を選択
      sprintln("got device, stop scan, connecting");
      ble.stopScan();
      ble.connect(params->peerAddr, BLEProtocol::AddressType::RANDOM_STATIC, NULL, NULL);
    }
  }
}

// 接続のコールバック処理
void connectionCallBack( const Gap::ConnectionCallbackParams_t *params ) {
  sprintln("* connection call back");
  ble.gattClient().launchServiceDiscovery(params->handle, NULL, discoveredCharacteristicCallBack);
  digitalWrite(13, LOW);       // LED OFF
}


// 切断のコールバック処理
void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  sprintln("* disconnect call back");
  ble.startScan(scanCallBack);
  digitalWrite(13, HIGH);      // LED ON
}

// 接続後のサービス列挙のコールバック処理
// 接続されたBLEのRSSIやUUIDを表示する
static void discoveredCharacteristicCallBack(const DiscoveredCharacteristic *chars) {
  sprintln("* servie enum call back");
  sprint("Chars UUID type:");
  sprintln(chars->getUUID().shortOrLong(), HEX);                // 0 16bit_uuid, 1 128bit_uuid
  sprint("Chars UUID: ");
  if(chars->getUUID().shortOrLong() == UUID::UUID_TYPE_SHORT) { //すでに決まっているUUIDを列挙
    sprintln(chars->getUUID().getShortUUID(), HEX);
  } else {                                                      //作ったUUIDを列挙
    uint8_t index;
    const uint8_t *uuid = chars->getUUID().getBaseUUID();
    for(index=0; index<16; index++) {
      sprint(uuid[index], HEX);
      sprint(" ");
    }
    sprintln("");
  }
  sprint("properties_read: ");
  sprintln(chars->getProperties().read(), DEC);
  sprint("properties_writeWoResp : ");
  sprintln(chars->getProperties().writeWoResp(), DEC);
  sprint("properties_write: ");
  sprintln(chars->getProperties().write(), DEC);
  sprint("properties_notify: ");
  sprintln(chars->getProperties().notify(), DEC);
  sprint("declHandle: ");
  sprintln(chars->getDeclHandle(), HEX);
  sprint("valueHandle: ");
  sprintln(chars->getValueHandle(), HEX);
  sprint("lastHandle: ");
  sprintln(chars->getLastHandle(), HEX);
  sprint("getConnectionHandle: ");
  sprintln(chars->getConnectionHandle(), HEX);
  sprintln("");

  // 読み書き可能なキャラクタリスティックかつ既定のハンドルのみ処理をする
  // nutは"2A00"(不明) or "2A06"(AlertLevel)が読み書き可能
  if (chars->getUUID().getShortUUID() == 0x2A06 && chars->getProperties().read() != 0 && chars->getProperties().write() != 0){
    sprintln("READ!!");
    Characteristic_values = *chars;        // 値を受信する
    ble.gattClient().read(chars->getConnectionHandle(), chars->getValueHandle(), 0);
  }
}

// 書き込みのコールバック処理
void onDataWriteCallBack(const GattWriteCallbackParams *params) {
  sprintln("* write call back");
  ble.disconnect((Gap::DisconnectionReason_t)0);
}

// 読み込みのコールバック処理
void onDataReadCallBack(const GattReadCallbackParams *params) {
  sprintln("* read call back");
  sprint("The handle : ");
  sprintln(params->handle, HEX);
  sprint("The offset : ");
  sprintln(params->offset, DEC);
  sprint("The len : ");
  sprintln(params->len, DEC);
  sprint("The data : ");
  for (uint8_t index = 0; index<params->len; index++) {
    sprint(params->data[index], HEX);
    sprint(" ");
  }
  sprintln("");

  if (params->data[0] == 0) {                         // 何も書かれていないときは、チェックポイントIDを書き込む
    uint8_t value = CHECK_POINT_ID;
    ble.gattClient().write(GattClient::GATT_OP_WRITE_REQ, Characteristic_values.getConnectionHandle(), params->handle, 1, (uint8_t *)&value);
  } else {
    ble.disconnect((Gap::DisconnectionReason_t)0);   // 既に書き込まれていたら何もしない
  }
}

// 初期処理
void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  sprintln("* setup");

  ble.init();
  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
  ble.onConnection(connectionCallBack);
  ble.onDisconnection(disconnectionCallBack);        // 切断されたときの処理
  ble.gattClient().onDataWrite(onDataWriteCallBack); // 書き込み send
  ble.gattClient().onDataRead(onDataReadCallBack);   // 読み込み read
  ble.setScanParams(5000, 200, 0, true);             // スキャニング時間 ウィンドウ時間? タイムアウト アクティブにするか
  ble.startScan(scanCallBack);                       // スキャニングのスタート
  digitalWrite(13, HIGH);                            // LED ON
}

// メイン
void loop() {
  ble.waitForEvent();
}

