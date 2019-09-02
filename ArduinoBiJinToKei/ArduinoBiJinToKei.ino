/* WiFi settings */
#define SSID_NAME "YourAP"
#define SSID_PASSWORD "YourPassword"

/* NTP server settings */
const char* ntpServer = "pool.ntp.org";
// const char* ntpServer = "192.168.123.1"; // local AP NTP server
#define GMT_OFFSET_SEC 28800L // Timezone +0800
#define DAYLIGHT_OFFSET_SEC 0L // no daylight saving

// select preferred BiJinToKei here
#include "URL.h"

#include <time.h>
#include <WiFi.h>
#include <esp_jpg_decode.h>
#include <SPI.h>
#include <HTTPClient.h>

/* display settings */
#include <Arduino_HWSPI.h>
#include <Arduino_GFX.h>    // Core graphics library by Adafruit
#include <Arduino_ST7789.h> // Hardware-specific library for ST7789 (with or without CS pin)
Arduino_HWSPI *bus = new Arduino_HWSPI(16 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ST7789 *tft = new Arduino_ST7789(bus, 17 /* RST */, 1 /* rotation */, false /* IPS */);
#define TFT_BL 22

static int len, offset;
static int8_t last_show_minute = -1;
static struct tm timeinfo;
static char url[1024];

HTTPClient http;

void setup()
{
  Serial.begin(115200);
  tft->begin();
  tft->fillScreen(BLACK);
  // tft->setAddrWindow(40, 30, WIDTH, HEIGHT);

  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, ntpServer);

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "NTP time: %A, %B %d %Y %H:%M:%S");

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  enableLoopWDT();
}

void loop()
{
  getLocalTime(&timeinfo);
  if (WiFi.status() != WL_CONNECTED)
  {
    // wait for WiFi connection
    delay(500);
  }
  else if (last_show_minute == timeinfo.tm_min)
  {
    delay(1000);
  }
  else
  {
    sprintf(url, URL, timeinfo.tm_hour, timeinfo.tm_min);
    Serial.println(url);

    Serial.print("[HTTP] begin...\n");
    http.begin(url);

    Serial.print("[HTTP] GET...\n");
    int httpCode = http.GET();

    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // HTTP header has been send and Server response header has been handled
    if (httpCode <= 0)
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    else
    {
      if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[HTTP] Not OK!\n");
      } else {
        // get lenght of document (is -1 when Server sends no Content-Length header)
        len = http.getSize();
        Serial.printf("[HTTP] size: %d\n", len);

        if (len <= 0) {
          Serial.printf("[HTTP] Unknow content size: %d\n", len);
        } else {
          // get tcp stream
          WiFiClient *http_stream = http.getStreamPtr();

          esp_jpg_decode(len, JPG_SCALE, http_stream_reader, tft_writer, http_stream /* arg */);
        }
      }
    }
    http.end();
    last_show_minute = timeinfo.tm_min;
  }
}

static size_t http_stream_reader(void *arg, size_t index, uint8_t *buf, size_t len)
{
  WiFiClient *http_stream = (WiFiClient*)arg;
  if (buf) {
    // Serial.printf("[HTTP] read: %d\n", len);
    size_t a = http_stream->available();
    size_t r = 0;
    while (r < len) {
      r += http_stream->readBytes(buf + r, min((len - r), a));
    }
    return r;
  } else {
    // Serial.printf("[HTTP] skip: %d\n", len);
    int l = len;
    while (l--) {
      http_stream->read();
    }
    return len;
  }
}

static bool tft_writer(void *arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
  if (data)
  {
    // Serial.printf("%d, %d, %d, %d\n", x, y, w, h);
    tft->startWrite();
    tft->writeAddrWindow(x, y, w, h);
    for (int i = 0; i < h; i++)
    {
      for (int j = 0; j < w; j++)
      {
        tft->writeColor(tft->color565(*(data++), *(data++), *(data++)));
      }
    }
    tft->endWrite();
  }

  // notify WDT still working
  feedLoopWDT();
  return true; // Continue to decompression
}