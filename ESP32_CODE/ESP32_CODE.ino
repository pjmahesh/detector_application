/*********
  mahesh@wisense.in - 24 Feb 2020
  
> Add Espressif boards https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
  File > Preferences > Additional Board Manager URLs
> Add Espressif board. Saerch Esp at Tools > Boards > Board Manager 
> Add https://github.com/me-no-dev/AsyncTCP to User > Documents > Arduono > library folder
> Add https://github.com/me-no-dev/ESPAsyncWebServer/ to User > Documents > Arduono > library folder
> Select board as "ESP32 Wrover Module" variable upload baudrates (or) "AI Thinker ESPCAM" for default 921600 upload baudrate.
> Select PORT. Select upload baudrate if selected "ESP32 Wrover Module" - 115200 if using Arduino/Launchpad as USB to TTL
> Connect
  TX(FTDI) to U0R(ESPCAM)
  RX(FTDI) to U0T(ESPCAM)
  I01(ESPCAM) to GND(ESPCAM)
  GND(FTDI) to GND(ESPCAM)
  3V/5V(FTDI) to 3V/5V(ESPCAM)
> Press and release reset button. Upload code.

Important to connect I01(ESPCAM) to GND(ESPCAM)

*********/

#include <WiFi.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <StringArray.h>
//#include <SPIFFS.h>
//#include <FS.h>
#include <HardwareSerial.h>

HardwareSerial uart(1);

boolean takeNewPhoto = false;  //Camera flag

//#define FILE_PHOTO "/photo.jpg"  // Photo File Name to save in SPIFFS

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
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
#define FLASH_LED          4
#define NOTIF_LED         33


void setup() {

  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, LOW);
  pinMode(NOTIF_LED, OUTPUT);
  digitalWrite(NOTIF_LED, HIGH);

  WiFi.mode(WIFI_OFF); //Turn off WiFi
  btStop();            //Turn off BT

  Serial.begin(115200);  // Serial port for debugging
  uart.begin(500000, SERIAL_8N1, 14, 15); //RX, TX

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Turn-off the 'brownout detector'

  // OV2640 camera module
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

  if (psramFound()) {
    Serial.print("psramFound");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config); // Camera init
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

}

void loop() {

  if (uart.available()>0)
  {
    digitalWrite(NOTIF_LED, HIGH); //NOTIF_LED IS ACTIVE LOW
    String msg = uart.readString();
    Serial.print("\nYou sent: ");
    Serial.println(msg);
    if (msg.equals("capture")){
       takeNewPhoto = true;
    }
  }
  
  if (takeNewPhoto) {
      digitalWrite(NOTIF_LED, LOW);
      capturePhotoWriteUart();
      takeNewPhoto = false;
      //sendViaUart(SPIFFS);
      digitalWrite(NOTIF_LED, HIGH);
  }
  
}



char localBuff[64];


void capturePhotoWriteUart( void ) {
  
    camera_fb_t * fb = NULL; // pointer
    bool ok = 0; // Boolean indicating if the picture has been taken correctly

    Serial.println("Taking a photo...");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    else
    {
      Serial.print("Image captured\nSize:");
      Serial.println(fb->len);

      sprintf(localBuff, "<%06d>", (int)fb->len);
      uart.write(localBuff);
      unsigned waitTime = millis();
      

      for (int idx=0; idx<fb->len; idx++)
      {
           unsigned char rxByte;

           if ((idx % 5000) == 0)
           {
               sprintf(localBuff, "sent %d bytes \n", idx);
               Serial.print(localBuff);
           }
           
           uart.write(fb->buf[idx]);

      }
    }

    esp_camera_fb_return(fb);


}
