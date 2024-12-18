#include "time.h"
#include <EEPROM.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// 아래 3개 라이브러리는 설치 필요
#include "UniversalTelegramBot.h"  //UniversalTelegramBot by Brian_Lough
#include <ArduinoJson.h>  //ArduinoJson by Benoit Blanchon 
#include "WiFiManager.h"  //WiFimanager by tzapu

// WiFiManager 구조체
WiFiManager wm;

// camera에서 입력된 내용을 저장할 구조체
camera_fb_t * fb = NULL;

// 멀티 태스킹 처리 변수
unsigned long previousMillis = 0;
unsigned long interval = 30000;
unsigned long currentMillis = 0;

// Telegram 접속에 필요한 정보 처리 변수
char def_BOTtoken[50];
char def_chat_id[50];
char devname[30];
String chat_id;
String BOTtoken;
String randomNum = String(esp_random());
String devstr =  "ESP32-"+randomNum.substring(0,4);

// 상태 확인 위한 임시 변수
int CheckNumber = 18;
bool flashState = false;

// 사진 전송 상태 확인 변수
bool sendPhoto = false;
bool dataAvailable = false;

// WiFi 및 Telegram 연결 관련 구조체
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

//ESP32-CAM camera 핀 번호 및 관련 변수
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN      4
bool lightState = LOW;

// 모션센서 상태 확인 변수
bool motionDetected = false;

// 모션센서 입력 핀
#define PIR_pin (gpio_num_t) 13

// 수신된 메시지 사이의 최소 시간
int botRequestDelay = 1000;
long lastTimeBotRan;

// NTP 상태 확인 임시 변수
int chkNTP = 0;

// 함수명 미리 정의
void handleNewMessages(int numNewMessages);
void sendPhotoTelegram();
void takePhoto();

// 모션 센서 인터럽트 처리 함수
void IRAM_ATTR detectsMovement(){
  motionDetected = true;
}

const char* ntpServer = "pool.ntp.org";     // NTP 서버
uint8_t timeZone = 9;                       // 한국 타임존 설정
uint8_t summerTime = 0;                     // 썸머타임 설정 없으면 0, 있으면 3600
struct tm timeinfo;                         // 시간 구조체 변수 선언

// 시간 출력 함수
void printLocalTime() {
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
    }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// 시간 동기화 함수
int get_NTP() { 
  configTime(3600 * timeZone, 3600 * summerTime, ntpServer);
  if(!getLocalTime(&timeinfo)){ 
    Serial.println("Failed to confirm present time");
    return 0;
  }
  else{
    Serial.print("Success check present time: ");
    printLocalTime();
    return 1;
  }
}

//------------------------------------------1번 절취선---------------------------------------------------

// ROM에 저장될 내용의 구조체
struct eprom_data {
  char devname[16];
  char chat_id[16];
  char BOTtoken[52];
  int eprom_good;
};

// ROM에 데이터 저장 함수
void do_eprom_write() {
  eprom_data ed;
  ed.eprom_good = CheckNumber;
  devstr.toCharArray(ed.devname, 12); 
  BOTtoken.toCharArray(ed.BOTtoken, 50);
  chat_id.toCharArray(ed.chat_id, 12);
//  Serial.println("Writing to EPROM ...");
  EEPROM.begin(200);
  EEPROM.put(0, ed);
  EEPROM.commit();
  EEPROM.end();
}

// ROM의 데이터 불러오기 함수
void do_eprom_read() {
  eprom_data ed;
  EEPROM.begin(200);
  EEPROM.get(0, ed);

  if (ed.eprom_good == CheckNumber) {
//    Serial.println("Good settings in the EPROM ");
    devstr = ed.devname;
    devstr.toCharArray(devname, 12);
    //devstr.length() + 1);
    BOTtoken = ed.BOTtoken;
    bot.updateToken(BOTtoken);
    chat_id = ed.chat_id;
//    Serial.println(devstr);
//    Serial.println(BOTtoken);
//    Serial.println(chat_id);
  } 
  else {
//    Serial.println("No settings in EPROM ");
    chat_id = "";  //더미 데이터 저장
    BOTtoken = "";  //더미 데이터 저장
    do_eprom_write();
    wm.resetSettings();
  }
}

// WiFiManager에서 불러온 정보 저장 함수
void saveParamCallback() {
  if (wm.server->hasArg("DevName")) {
    String sDevName  = wm.server->arg("DevName");
    devstr = sDevName;
//    Serial.println(sDevName);
  }
  if (wm.server->hasArg("chat_id")) {
    String schat_id  = wm.server->arg("chat_id");
//    Serial.println(schat_id);
    chat_id = schat_id;
  }
  if (wm.server->hasArg("BOTtoken")) {
    String sBOTtoken  = wm.server->arg("BOTtoken");
//    Serial.println(sBOTtoken);
    BOTtoken = sBOTtoken;
    bot.updateToken(BOTtoken);
  }
  do_eprom_write();
}

// WiFi 실행 함수
bool init_wifi() {
  do_eprom_read();
  devstr.toCharArray(devname, devstr.length() + 1);
  BOTtoken.toCharArray(def_BOTtoken, BOTtoken.length() + 1);
  chat_id.toCharArray(def_chat_id, chat_id.length() + 1);
  
  WiFiManagerParameter dev("DevName", "Name of Device", devname, 12);
  wm.addParameter(&dev);
  WiFiManagerParameter id("chat_id", "Telegram ID", def_chat_id, 15);
  wm.addParameter(&id);
  WiFiManagerParameter pass ("BOTtoken", "Telegram Pass", def_BOTtoken, 50);
  wm.addParameter(&pass);
    
  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"wifi", "info", "sep", "restart", "exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  bool res;

  wm.setConnectTimeout(60 * 5); // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(60 * 5); // auto close configportal after n seconds

  res = wm.autoConnect(devname); // use the devname defined above, with no password

  if (res) {
    Serial.println("Succesful Connection using WiFiManager");
    do_eprom_write();
  } else {
    Serial.println("Connection failed using WiFiManager - not started Web services");
    delay(1000);
    ESP.restart();
  }

//  Serial.println("");
//  Serial.println("WiFi connected");
  return true;
}

// Telegram에 전송할 내용 확인 함수
bool isMoreDataAvailable() {
  if (dataAvailable) {
    dataAvailable = false;
    return true;
  }
  else {
    return false;
  }
}

// 버퍼에서 다음 사진 불러오기 함수
byte *getNextBuffer() {
  if (fb) {
    return fb->buf;
  }
  else {
    return nullptr;
  }
}

// 버퍼에 있는 데이터 길이 확인 함수
int getNextBufferLen() {
  if (fb) {
    return fb->len;
  }
  else {
    return 0;
  }
}

//------------------------------------------2번 절취선---------------------------------------------------

// Telegram으로 사진 전송 함수
void sendPhotoTelegram() {
  if (flashState == true){
    digitalWrite(FLASH_LED_PIN, HIGH);
  }
  fb = esp_camera_fb_get();  
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL;
  // Take Picture with Camera
  delay(100);
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    bot.sendMessage(chat_id, "Camera capture failed", "");
    return;
  }
  dataAvailable = true;
  Serial.println("Sending");
  String sent = bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len, isMoreDataAvailable, nullptr, getNextBuffer, getNextBufferLen);
  esp_camera_fb_return(fb);
  delay(100);
  fb = esp_camera_fb_get();
  dataAvailable = true;
  if (flashState == true){
    digitalWrite(FLASH_LED_PIN, LOW);
  }
  bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len, isMoreDataAvailable, nullptr, getNextBuffer, getNextBufferLen);
  if (sent) {
    Serial.println("successfully sent");
  }
  else {
    Serial.println("Sending fail");
  }
  esp_camera_fb_return(fb);
  fb = NULL;
}

// Telegram에서 수신한 메세지 처리 함수
void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++){
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != chat_id){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String fromName = bot.messages[i].from_name;
    if (text == "/flash") {
      flashState = !flashState;
      if(flashState == true){
        bot.sendMessage(chat_id, "Flash on", "");
      }
      if(flashState == false){
        bot.sendMessage(chat_id, "Flash off", "");
      }
    }
    if (text == "/light") {
      lightState = !lightState;
      digitalWrite(FLASH_LED_PIN, lightState);
      if(lightState == true){
        bot.sendMessage(chat_id, "Light on", "");
      }
      if(lightState == false){
        bot.sendMessage(chat_id, "Light off", "");
      }
    }
    if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
    if (text == "/start"){
      String welcome = "Welcome to the ESP32-CAM Telegram bot.\n";
      welcome += "/photo : takes a new photo\n";
      welcome += "/light : toggle LED on and off\n";
//      welcome += "/readings : request sensor readings\n";
      welcome += "/flash : toggle flash LED when take photo, on and off\n";
      welcome += "You'll receive a photo whenever motion is detected.\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

esp_err_t err = NULL;

// 0번 CPU에 인터럽트 할당
void Interrupt_Task(void *pvParameters) {
//  Serial.print("* Interrupt_Task() is running on core ");
//  Serial.println(xPortGetCoreID());
  attachInterrupt(PIR_pin, detectsMovement, CHANGE);
  while(true){
    vTaskDelay(500);
  }
  vTaskDelete(NULL);
}

//------------------------------------------3번 절취선---------------------------------------------------

// 1번 CPU에 작업 할당
void Loop_Task(void *pvParameters) {
//  Serial.print("* Loop_Task() is running on core ");
//  Serial.println(xPortGetCoreID());
  while(true){
    // 시간 동기화 실패시 재실행
    currentMillis = millis();
    if ((chkNTP == 0) && (currentMillis - previousMillis >= interval)) {
      chkNTP = get_NTP();
      previousMillis = currentMillis;
    }
    // 사진 전송
    if (sendPhoto){
      Serial.println("Preparing photo");
      sendPhotoTelegram(); 
      sendPhoto = false; 
    }
    // 모션 감지 작동
    if(motionDetected == true){
      motionDetected = false;
      Serial.println("Motion Detected");
      sendPhotoTelegram();
    }
    // 메시지 수신
    if (millis() > lastTimeBotRan + botRequestDelay){
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages){
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
    }
  }
  vTaskDelete(NULL);
}
  
void setup() {
  Serial.begin(115200);
//  Serial.print("* setup() is running on core ");
//  Serial.println(xPortGetCoreID());

  // brownout detector(저전압 감지 기능) 비활성화 "soc/soc.h", "soc/rtc_cntl_reg.h"에 정의
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  // 핀 모드 설정
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, lightState);
  pinMode(PIR_pin, INPUT_PULLUP);

  //WiFi 시작
  init_wifi();
  
  // Telegram 연결 인증 설정
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); 

  // camera 설정
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // RAM에 따른 사진 화질 설정 
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 20;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 20;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera 작동 상태 확인
  err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // camera 해상도 조정
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  // 시간 동기화
  chkNTP = get_NTP();

  // 멀티태스킹 tlwkr
  xTaskCreatePinnedToCore(Loop_Task, "Loop Task1", 10000, NULL, 1, NULL, 1);
  delay(500);
  xTaskCreatePinnedToCore(Interrupt_Task, "Interrupt_Task1", 10000, NULL, 1, NULL, 0);
  delay(500);
}

void loop(){
}
