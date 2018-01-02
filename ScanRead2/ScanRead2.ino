#include <nRF5x_BLE_API.h>

//#define DEBUG

#ifdef DEBUG
#define sprintln Serial.println
#define sprint Serial.print
#else
#define sprintln(fmt, ...) ;
#define sprint(fmt, ...) ;
#endif

BLE           ble;
static uint8_t CHECK_POINT_ID = 0x11;
//ServerのUUID
static uint8_t service1_uuid[]    = {0x71, 0x3D, 0, 0, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
UUID service_uuid(service1_uuid);


// To save the hrm characteristic and descriptor
static DiscoveredCharacteristic            Characteristic_values;

//関数
static void scanCallBack(const Gap::AdvertisementCallbackParams_t *params);
static void discoveredCharacteristicCallBack(const DiscoveredCharacteristic *chars);

static void onDataWriteCallBack(const GattWriteCallbackParams *params); //送信のときに呼ぶ
static void onDataReadCallBack(const GattReadCallbackParams *params); //静的なデータを取得するときに呼ぶ


/**
 * 接続判定をする関数
 */
uint32_t ble_advdata_parser(uint8_t type, uint8_t advdata_len, uint8_t *p_advdata, uint8_t *len, uint8_t *p_field_data) {
  uint8_t index=0;
  uint8_t field_length, field_type;

  while(index<advdata_len) {
    field_length = p_advdata[index];
    field_type   = p_advdata[index+1];
    if(field_type == type) {
      memcpy(p_field_data, &p_advdata[index+2], (field_length-1));
      *len = field_length - 1;
      return NRF_SUCCESS;
    }
    index += field_length + 1;
  }
  return NRF_ERROR_NOT_FOUND;
}


/**
 * スキャンの後、ショートネームが一致したら接続
 */
static void scanCallBack(const Gap::AdvertisementCallbackParams_t *params) {
  uint8_t len;
  uint8_t adv_name[31];
  if( NRF_SUCCESS == ble_advdata_parser(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, params->advertisingDataLen, (uint8_t *)params->advertisingData, &len, adv_name) ) {
    sprint("Short name len : ");
    sprintln(len, DEC);
    sprint("Short name is : ");
    sprintln((const char*)adv_name);

    if( memcmp("TXRX", adv_name, 4) == 0x00 ) {//取得したいショートネームを書く
      sprintln("Got device, stop scan ");
      ble.stopScan();
      ble.connect(params->peerAddr, BLEProtocol::AddressType::RANDOM_STATIC, NULL, NULL);
    }
  }
}

/** 
 * 接続した後、振る舞い(write, read etc..)の情報を取得
 * discoveredCharacteristicCallBack関数で表示をする
 */
void connectionCallBack( const Gap::ConnectionCallbackParams_t *params ) {
  sprintln("\r\n----startDiscovery");
  //オプションつけると自作したサービスのみが呼ばれるようになる
  ble.gattClient().launchServiceDiscovery(params->handle, NULL, discoveredCharacteristicCallBack, service_uuid);
}


/*
 * 切断のときに呼ばれる
 */
void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  sprintln("Disconnected, start to scanning");
  ble.startScan(scanCallBack);
}

/**
 * 接続されたBLEのRSSIやUUIDを表示する
 */
static void discoveredCharacteristicCallBack(const DiscoveredCharacteristic *chars) {
  //Info
  sprint("Chars UUID type:");
  sprintln(chars->getUUID().shortOrLong(), HEX);// 0 16bit_uuid, 1 128bit_uuid
  sprint("Chars UUID: ");
  if(chars->getUUID().shortOrLong() == UUID::UUID_TYPE_SHORT) {//すでに決まっているUUIDを列挙
    sprintln(chars->getUUID().getShortUUID(), HEX);
  }
  else {//作ったUUIDを列挙
    uint8_t index;
    const uint8_t *uuid = chars->getUUID().getBaseUUID();
    for(index=0; index<16; index++) {
      sprint(uuid[index], HEX);
      sprint(" ");
    }
    sprintln(" ");
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

/*  //notifyが許可されているか
  if(chars->getProperties().notify() == 0x01){
    Serial.println("found notify");
    Characteristic_values = *chars;
  }*/
  if (chars->getValueHandle() == 0x0E){
    Characteristic_values = *chars;
    ble.gattClient().read(chars->getConnectionHandle(), chars->getValueHandle(), 0);
  }
}

/**
 * データをPeripheralへ送信
 * Peripheralに何かしてほしいときはこの関数をコールバックする。
 */
void onDataWriteCallBack(const GattWriteCallbackParams *params) {
  sprintln("GattClient write call back ");
  ble.disconnect((Gap::DisconnectionReason_t)0);
}

/** @brief  read callback handle
 *データの読み込み
 *Peripheralからのデータを読み込み
 *notifyと比較して、静的なデータのみを読み込む
 */
void onDataReadCallBack(const GattReadCallbackParams *params) {
  sprintln("*****GattClient read call back ");
  sprint("The handle : ");
  sprintln(params->handle, HEX);
  sprint("The offset : ");
  sprintln(params->offset, DEC);
  sprint("The len : ");
  sprintln(params->len, DEC);
  sprint("The data : ");
  for (uint8_t index=0; index<params->len; index++) {
    sprint(params->data[index]); //これでデータを取得する
    sprint(" ");
  }
  sprintln("");

  if (params->data[0] ==0) {
    uint8_t value = CHECK_POINT_ID;
    ble.gattClient().write(GattClient::GATT_OP_WRITE_REQ, Characteristic_values.getConnectionHandle(), params->handle, 1, (uint8_t *)&value);
  } else {
    ble.disconnect((Gap::DisconnectionReason_t)0);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  sprintln("BLE Central Demo ");

  ble.init();
  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
  ble.onConnection(connectionCallBack);
  ble.onDisconnection(disconnectionCallBack); //切断されたときの処理
  ble.gattClient().onDataWrite(onDataWriteCallBack); //書き込み send
  ble.gattClient().onDataRead(onDataReadCallBack); //読み込み read
  ble.setScanParams(1000, 200, 0, true); //スキャニング時間 ウィンドウ時間? タイムアウト アクティブにするか
  // start scanning
  ble.startScan(scanCallBack); //スキャニングのスタート
}

void loop() {
  // put your main code here, to run repeatedly:
  ble.waitForEvent();
}

