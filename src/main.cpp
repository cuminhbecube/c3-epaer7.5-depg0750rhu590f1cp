#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <math.h>

#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "../driver_config.h"
#include "../include/weather_icons.h"
#include "ImagePage.h"
#include "WifiSetupPage.h"

// --- CONFIG ---
const char* FIRMWARE_URL = "https://raw.githubusercontent.com/cuminhbecube/c3-epaer7.5-depg0750rhu590f1cp/main/firmware-ota/firmware.bin";

// --- CONFIG STORE ---
char config_lat[16] = "21.02";
char config_lon[16] = "105.83";
uint8_t config_rotation = 0; // 0=0°, 1=90°, 2=180°(default), 3=270°
bool shouldSaveConfig = false;

static uint8_t rotDegToIdx(int deg) {
    if (deg == 90)  return 1;
    if (deg == 180) return 2;
    if (deg == 270) return 3;
    return 0;
}
static int rotIdxToDeg(uint8_t idx) {
    if (idx == 1) return 90;
    if (idx == 2) return 180;
    if (idx == 3) return 270;
    return 0;
}

void saveConfigCallback() {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void loadConfig() {
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            size_t size = configFile.size();
            std::unique_ptr<char[]> buf(new char[size]);
            configFile.readBytes(buf.get(), size);
            configFile.close();

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, buf.get());
            if (!error) {
                if (doc["lat"].is<const char*>()) strlcpy(config_lat, doc["lat"], sizeof(config_lat));
                if (doc["lon"].is<const char*>()) strlcpy(config_lon, doc["lon"], sizeof(config_lon));
                if (doc["rotation"].is<int>()) config_rotation = (uint8_t)constrain(doc["rotation"].as<int>(), 0, 3);
                Serial.println("Config Loaded");
            }
        }
    } else {
        Serial.println("No Config File, using defaults");
    }
}

void saveConfig() {
    Serial.println("Saving Config...");
    JsonDocument doc;
    doc["lat"] = config_lat;
    doc["lon"] = config_lon;
    doc["rotation"] = config_rotation;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("Config Saved");
}

// --- TIMERS ---
unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;
const unsigned long TIME_UPDATE_INTERVAL    = 60000;
const unsigned long WEATHER_UPDATE_INTERVAL = 3600000; // 1 Hour

// --- DISPLAY (3C 640x384) ---
// Full HEIGHT buffer: GxEPD2 loads all pixel data at once → single full refresh
// instead of 4x partial page updates. ESP32-C3 has 327KB RAM, can handle it.
#include "epd3c/GxEPD2_750c.h"
GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

// --- WEB SERVER ---
WebServer server(80);
bool inImageMode = false;
bool imageUpdatePending = false;
File uploadFile;

// --- STRUCTS ---
struct DailyForecast {
    time_t date;
    int code;
    float maxTemp;
    float minTemp;
};

struct WeatherData {
    DailyForecast daily[5];
    float currentTemp;
    int currentCode;
    int dailyCount;
    bool valid;
};
WeatherData weather = {{{0}}, 0, 0, 0, false};
bool showImageMode = false;

// --- IMAGE SLIDESHOW ---
#define MAX_IMAGES 5
int  currentImageIndex = 0;
unsigned long lastImageSwitch = 0;
const unsigned long IMAGE_SWITCH_INTERVAL = 5UL * 60UL * 1000UL; // 5 minutes

String imgPath(int idx) {
    char p[12]; snprintf(p, sizeof(p), "/img%d.bin", idx);
    return String(p);
}

int countImages() {
    int n = 0;
    for (int i = 0; i < MAX_IMAGES; i++)
        if (LittleFS.exists(imgPath(i))) n++;
    return n;
}



// --- BUTTON ---
const int BTN_PIN = PIN_SWITCH_1;
int lastBtnState = HIGH;
unsigned long pressStartTime = 0;
bool isPressing = false;
unsigned long lastReleaseTime = 0;   // thời điểm nhả nút lần trước
int pressCount = 0;                  // số lần nhấn liên tiếp trong khoảng DOUBLE_PRESS_GAP
const unsigned long DOUBLE_PRESS_GAP = 400; // ms tối đa giữa 2 lần nhấn

// --- NTP ---
const char* ntpServer  = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.cloudflare.com";
const long  gmtOffset_sec      = 7 * 3600;
const int   daylightOffset_sec = 0;

// --- PROTOTYPES ---
void drawScreen();
void drawScreenPortrait();
void drawTestImage();
void handleImageUpload();
void startImageServer();
void fetchWeather();
void performOTA();
void enterConfigMode();
void showMessage(String msg, String subMsg);
const unsigned char* getIcon(int code);

// FreeRTOS task chạy song song với blocking WiFiManager portal
// Nhấn 5x → deep sleep ngay lập tức mà không cần thoát portal
static void btnDeepSleepTask(void*) {
    int state = HIGH;
    unsigned long ps = 0, lr = 0;
    int cnt = 0;
    while (true) {
        int r = digitalRead(BTN_PIN);
        if (r == LOW  && state == HIGH) ps = millis();
        if (r == HIGH && state == LOW) {
            if (millis() - ps < 3000) {
                if (millis() - lr <= DOUBLE_PRESS_GAP) cnt++; else cnt = 1;
                lr = millis();
            } else cnt = 0;
        }
        state = r;
        if (cnt >= 5 && r == HIGH && (millis() - lr > DOUBLE_PRESS_GAP)) {
            cnt = 0;
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            display.setFullWindow(); display.firstPage();
            do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
            display.hibernate();
            esp_deep_sleep_start(); // không quay lại
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- BOOTING (ESP32-C3) ---");

    pinMode(BTN_PIN, INPUT_PULLUP);

    // Init LittleFS (true = format on fail)
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS Mount Failed");
    }

    // Init SPI with custom pins, then init display
    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
    display.init(115200, true, 10, false);
    display.setRotation(2);
    display.setTextColor(GxEPD_BLACK);

    // Hold button at boot -> Image Mode
    if (digitalRead(BTN_PIN) == LOW) {
        startImageServer();
        inImageMode = true;
        // Button state for 5x deep sleep
        static int btnBoot = HIGH;
        static unsigned long psBoot = 0;
        static int cntBoot = 0;
        static unsigned long lrBoot = 0;
        while (1) {
            server.handleClient();
            if (imageUpdatePending) {
                delay(200);
                handleImageUpload();
                imageUpdatePending = false;
            }
            // 5x press → deep sleep
            int rb = digitalRead(BTN_PIN);
            if (rb == LOW  && btnBoot == HIGH) psBoot = millis();
            if (rb == HIGH && btnBoot == LOW) {
                if (millis() - psBoot < 3000) {
                    if (millis() - lrBoot <= DOUBLE_PRESS_GAP) cntBoot++; else cntBoot = 1;
                    lrBoot = millis();
                } else { cntBoot = 0; }
            }
            btnBoot = rb;
            if (cntBoot >= 5 && rb == HIGH && (millis() - lrBoot > DOUBLE_PRESS_GAP)) {
                cntBoot = 0;
                WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
                display.setFullWindow(); display.firstPage();
                do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
                display.hibernate();
                esp_deep_sleep_start();
            }
            delay(2);
        }
    }

    loadConfig();
    display.setRotation(config_rotation);

    WiFiManager wm;
    wm.setCustomHeadElement(WM_HEAD_EXTRA);
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(300); // 5 phút config portal, sau đó tự thoát

    char rotDegStr[4];
    snprintf(rotDegStr, sizeof(rotDegStr), "%d", rotIdxToDeg(config_rotation));
    WiFiManagerParameter custom_lat("lat", "Latitude (Vi do)", config_lat, 16);
    WiFiManagerParameter custom_lon("lon", "Longitude (Kinh do)", config_lon, 16);
    WiFiManagerParameter custom_rot("rot", "Rotation (0/90/180/270)", rotDegStr, 4);
    wm.addParameter(&custom_lat);
    wm.addParameter(&custom_lon);
    wm.addParameter(&custom_rot);
    wm.setSaveConfigCallback(saveConfigCallback);
    // Hiển thị màn hình WIFI SETUP khi portal mở
    wm.setAPCallback([](WiFiManager*) {
        showMessage("WIFI SETUP", "AP: BECUBE-CLOCK\n192.168.4.1");
    });

    // Task FreeRTOS theo dõi 5x press trong khi portal blocking
    TaskHandle_t wmBtnTask = nullptr;
    xTaskCreate(btnDeepSleepTask, "wm_btn", 1024, nullptr, 1, &wmBtnTask);

    Serial.println("[WiFi] Connecting...");
    bool wmConnected = wm.autoConnect("BECUBE-CLOCK");

    if (wmBtnTask) { vTaskDelete(wmBtnTask); wmBtnTask = nullptr; }

    if (!wmConnected) {
        // Hết 5 phút → offline
        Serial.println("[WiFi] No connection after 5 min — switching to offline/slideshow mode");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        display.setRotation(config_rotation);
        if (countImages() > 0) {
            showImageMode = true;
            currentImageIndex = 0;
            while (!LittleFS.exists(imgPath(currentImageIndex)) && currentImageIndex < MAX_IMAGES - 1)
                currentImageIndex++;
            handleImageUpload();
            lastImageSwitch = millis();
        } else {
            drawScreen();
        }
        // Vòng lặp offline: chỉ xử lý nút, không fetch weather
        while (1) {
            int r = digitalRead(BTN_PIN);
            static int lastBtnOffline = HIGH;
            static unsigned long pressStartOffline = 0;
            static int offlinePressCount = 0;
            static unsigned long offlineLastRelease = 0;
            if (r == LOW && lastBtnOffline == HIGH) pressStartOffline = millis();
            if (r == HIGH && lastBtnOffline == LOW) {
                unsigned long dur = millis() - pressStartOffline;
                if (dur >= 8000) {
                    offlinePressCount = 0;
                    // Giữ 8s → vào Image Server mode để upload ảnh
                    startImageServer();
                    inImageMode = true;
                    static int btnIs = HIGH; static unsigned long psIs = 0;
                    static int cntIs = 0;    static unsigned long lrIs = 0;
                    while (1) {
                        server.handleClient();
                        if (imageUpdatePending) {
                            delay(200);
                            handleImageUpload();
                            imageUpdatePending = false;
                            showImageMode = true;
                            server.stop();
                            inImageMode = false;
                            WiFi.softAPdisconnect(true);
                            WiFi.mode(WIFI_OFF);
                        }
                        // 5x press → deep sleep
                        int rr = digitalRead(BTN_PIN);
                        if (rr == LOW  && btnIs == HIGH) psIs = millis();
                        if (rr == HIGH && btnIs == LOW) {
                            if (millis() - psIs < 3000) {
                                if (millis() - lrIs <= DOUBLE_PRESS_GAP) cntIs++; else cntIs = 1;
                                lrIs = millis();
                            } else { cntIs = 0; }
                        }
                        btnIs = rr;
                        if (cntIs >= 5 && rr == HIGH && (millis() - lrIs > DOUBLE_PRESS_GAP)) {
                            cntIs = 0;
                            WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
                            display.setFullWindow(); display.firstPage();
                            do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
                            display.hibernate();
                            esp_deep_sleep_start();
                        }
                        delay(2);
                    }
                } else if (dur >= 15000) {
                    offlinePressCount = 0;
                    enterConfigMode(); // reset WiFi
                } else if (dur < 3000) {
                    unsigned long now_ms = millis();
                    if (now_ms - offlineLastRelease <= DOUBLE_PRESS_GAP) offlinePressCount++;
                    else offlinePressCount = 1;
                    offlineLastRelease = now_ms;
                }
            }
            lastBtnOffline = r;
            // Deferred multi-press action
            if (offlinePressCount > 0 && r == HIGH &&
                (millis() - offlineLastRelease > DOUBLE_PRESS_GAP)) {
                int cnt = offlinePressCount;
                offlinePressCount = 0;
                if (cnt >= 5) {
                    Serial.println("[Offline] 5x -> Deep Sleep");
                    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
                    display.setFullWindow(); display.firstPage();
                    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
                    display.hibernate();
                    esp_deep_sleep_start();
                } else if (cnt == 1) {
                    // Nhấn ngắn: chuyển ảnh tiếp theo (nếu đang slideshow)
                    if (showImageMode && countImages() > 1) {
                        int next = (currentImageIndex + 1) % MAX_IMAGES;
                        int tries = 0;
                        while (!LittleFS.exists(imgPath(next)) && tries < MAX_IMAGES) {
                            next = (next + 1) % MAX_IMAGES;
                            tries++;
                        }
                        if (LittleFS.exists(imgPath(next))) {
                            currentImageIndex = next;
                            handleImageUpload();
                            lastImageSwitch = millis();
                        }
                    }
                }
            }
            // Slideshow tự động mỗi 5 phút
            if (showImageMode && countImages() > 1 &&
                (millis() - lastImageSwitch > IMAGE_SWITCH_INTERVAL)) {
                int next = (currentImageIndex + 1) % MAX_IMAGES;
                int tries = 0;
                while (!LittleFS.exists(imgPath(next)) && tries < MAX_IMAGES) {
                    next = (next + 1) % MAX_IMAGES;
                    tries++;
                }
                if (LittleFS.exists(imgPath(next))) {
                    currentImageIndex = next;
                    handleImageUpload();
                    lastImageSwitch = millis();
                }
            }
            delay(50);
        }
    } else {
        Serial.println("[WiFi] Connected");
    }

    if (shouldSaveConfig) {
        strcpy(config_lat, custom_lat.getValue());
        strcpy(config_lon, custom_lon.getValue());
        config_rotation = rotDegToIdx(atoi(custom_rot.getValue()));
        display.setRotation(config_rotation);
        saveConfig();
    }

    // NTP Sync — up to 40s (80 × 500ms). DNS on first boot can be slow.
    Serial.println("[Time] Syncing NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, ntpServer2, ntpServer3);

    int ntpRetry = 0;
    while (time(nullptr) < 1000000000 && ntpRetry < 80) {
        Serial.print(".");
        delay(500);
        ntpRetry++;
    }
    Serial.println();

    time_t nowTs = time(nullptr);
    Serial.printf("[Time] Epoch: %ld\n", nowTs);
    if (nowTs > 1000000000) Serial.println(ctime(&nowTs));
    else                     Serial.println("[Time] NTP Failed");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Setup] Fetching Weather...");
        fetchWeather();
        delay(500);
        // If fetch failed, retry after 1 minute instead of waiting the full 1-hour interval
        if (!weather.valid) {
            Serial.println("[Setup] Weather fetch failed, will retry in 1 min");
            lastWeatherUpdate = millis() - WEATHER_UPDATE_INTERVAL + 60000UL;
        }
    }

    Serial.println("[Setup] Drawing Screen...");
    drawScreen();
    Serial.println("[Setup] Complete!");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
    // --- BUTTON ---
    int reading = digitalRead(BTN_PIN);

    if (reading == LOW && lastBtnState == HIGH) {
        pressStartTime = millis();
        isPressing = true;
    }

    if (reading == HIGH && lastBtnState == LOW) {
        unsigned long duration = millis() - pressStartTime;
        isPressing = false;
        Serial.printf("[Button] Released. Duration: %lu ms\n", duration);

        if (duration < 3000) {
            // Đếm số lần nhấn ngắn, chưa xử lý ngay
            unsigned long now_ms = millis();
            if (now_ms - lastReleaseTime <= DOUBLE_PRESS_GAP) {
                pressCount++;
            } else {
                pressCount = 1;
            }
            lastReleaseTime = now_ms;
        } else if (duration < 8000) {
            pressCount = 0;
            Serial.println("[Mode] Entering Image Server Mode");
            startImageServer();
            inImageMode = true;
        } else if (duration < 15000) {
            pressCount = 0;
            Serial.println("[Mode] Starting OTA");
            performOTA();
        } else {
            pressCount = 0;
            Serial.println("[Mode] Resetting WiFi Config");
            enterConfigMode();
        }
    }
    lastBtnState = reading;

    // --- Xử LÝ DEFERRED: sau DOUBLE_PRESS_GAP kể từ lần nhấn cuối ---
    if (pressCount > 0 && !isPressing &&
        (millis() - lastReleaseTime > DOUBLE_PRESS_GAP)) {

        int count = pressCount;
        pressCount = 0;
        Serial.printf("[Button] Action: count=%d\n", count);

        if (count >= 5) {
            // Xóa trắng màn hình và vào deep sleep
            Serial.println("[Button] 5x -> Deep Sleep");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            display.setFullWindow();
            display.firstPage();
            do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
            display.hibernate();
            esp_deep_sleep_start();

        } else if (count == 2) {
            // Double-press: chuyển ảnh tiếp theo (bất kỳ chế độ nào, miễn là đang slideshow và có ≥ 2 ảnh)
            if (showImageMode && countImages() > 1) {
                Serial.println("[Button] Double-press -> Next image");
                int next = (currentImageIndex + 1) % MAX_IMAGES;
                int tries = 0;
                while (!LittleFS.exists(imgPath(next)) && tries < MAX_IMAGES) {
                    next = (next + 1) % MAX_IMAGES;
                    tries++;
                }
                if (LittleFS.exists(imgPath(next))) {
                    currentImageIndex = next;
                    handleImageUpload();
                    lastImageSwitch = millis();
                }
            }

        } else if (count == 1) {
            // Nhấn 1 lần: luôn toggle slideshow ảnh (bất kỳ chế độ nào)
            if (inImageMode) {
                // Đang ở AP upload mode → thoát về STA rồi vào slideshow nếu có ảnh
                Serial.println("[Mode] Exiting AP Mode -> STA");
                inImageMode = false;
                server.stop();
                WiFi.mode(WIFI_STA);
                WiFi.begin();
                if (countImages() > 0) {
                    showImageMode = true;
                    currentImageIndex = 0;
                    while (!LittleFS.exists(imgPath(currentImageIndex)) && currentImageIndex < MAX_IMAGES - 1)
                        currentImageIndex++;
                    handleImageUpload();
                    lastImageSwitch = millis();
                } else {
                    showMessage("RECONNECTING", "Wait for WiFi...");
                    lastWeatherUpdate = 0;
                }
            } else if (countImages() > 0) {
                showImageMode = !showImageMode;
                if (showImageMode) {
                    currentImageIndex = 0;
                    while (!LittleFS.exists(imgPath(currentImageIndex)) && currentImageIndex < MAX_IMAGES - 1)
                        currentImageIndex++;
                    handleImageUpload();
                    lastImageSwitch = millis();
                } else {
                    drawScreen();
                }
            } else {
                fetchWeather();
                drawScreen();
            }
        }
        // count 3, 4: bỏ qua
    }

    // --- IMAGE MODE LOOP ---
    if (inImageMode) {
        server.handleClient();

        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 3000) {
            Serial.printf("[Loop] inImageMode=true, Pending=%d, Clients=%d\n",
                imageUpdatePending, WiFi.softAPgetStationNum());
            lastDebug = millis();
        }

        if (imageUpdatePending) {
            Serial.println("[Loop] Image Pending! Drawing...");
            delay(200);
            handleImageUpload();
            imageUpdatePending = false;
            showImageMode = true;
            Serial.println("[Loop] Image Processed. Exiting AP mode...");
            // Thoát Image Mode, quay lại STA và kết nối WiFi
            server.stop();
            inImageMode = false;
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            WiFi.begin();
            lastWeatherUpdate = millis() - WEATHER_UPDATE_INTERVAL + 30000UL; // fetch weather sau 30s
        }
        return;
    }

    // --- AUTO UPDATE ---
    unsigned long now = millis();

    if (!isPressing) {
        if (!showImageMode && (now - lastTimeUpdate > TIME_UPDATE_INTERVAL)) {
            // Only auto-refresh for clock/weather. Static image does NOT auto-redraw
            // (E-Ink 3C takes ~50s to refresh; redrawing every 60s creates infinite loop)
            drawScreen();
            lastTimeUpdate = millis(); // Reset AFTER draw completes (draw can take 50s)
        }
        // Slideshow: rotate through stored images every 5 minutes
        if (showImageMode && countImages() > 1 && (now - lastImageSwitch > IMAGE_SWITCH_INTERVAL)) {
            int next = (currentImageIndex + 1) % MAX_IMAGES;
            int tries = 0;
            while (!LittleFS.exists(imgPath(next)) && tries < MAX_IMAGES) {
                next = (next + 1) % MAX_IMAGES;
                tries++;
            }
            if (LittleFS.exists(imgPath(next))) {
                currentImageIndex = next;
                handleImageUpload();
                lastImageSwitch = millis();
            }
        }
        if (now - lastWeatherUpdate > WEATHER_UPDATE_INTERVAL) {
            lastWeatherUpdate = now;
            if (WiFi.status() == WL_CONNECTED) fetchWeather();
        }
    }
    delay(50);
}

// =============================================================================
// HELPERS
// =============================================================================
const unsigned char* getIcon(int code) {
    if (code == 0 || code == 1)             return icon_sun_64;           // clear / mainly clear
    if (code == 2)                          return icon_partly_cloudy_64; // partly cloudy
    if (code == 3)                          return icon_cloud_64;          // overcast
    if (code >= 45 && code <= 48)           return icon_fog_64;            // fog / rime fog
    if (code >= 51 && code <= 57)           return icon_drizzle_64;        // drizzle
    if (code >= 61 && code <= 67)           return icon_rain_64;           // rain / freezing rain
    if (code >= 71 && code <= 77)           return icon_snow_64;           // snow / snow grains
    if (code >= 80 && code <= 82)           return icon_rain_64;           // rain showers
    if (code >= 85 && code <= 86)           return icon_snow_64;           // snow showers
    if (code >= 95)                         return icon_thunder_64;        // thunderstorm
    return icon_cloud_64;
}

void showMessage(String msg, String subMsg) {
    int W = display.width();
    int H = display.height();
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        int16_t bx, by; uint16_t bw, bh;

        // ── Main title ────────────────────────────────
        display.setFont(&FreeSansBold24pt7b);
        display.getTextBounds(msg.c_str(), 0, 0, &bx, &by, &bw, &bh);
        display.setCursor((W - (int)bw) / 2, H / 2 - 10);
        display.print(msg);

        // ── Subtitle — split on '\n' ───────────────────
        display.setFont(&FreeSansBold12pt7b);
        const int lineH = 28;
        int nLines = 1;
        for (int i = 0; i < (int)subMsg.length(); i++) if (subMsg[i] == '\n') nLines++;
        int lineY = H / 2 + (nLines > 1 ? 28 : 38);
        int start = 0;
        for (int i = 0; i <= (int)subMsg.length(); i++) {
            if (i == (int)subMsg.length() || subMsg[i] == '\n') {
                String line = subMsg.substring(start, i);
                display.getTextBounds(line.c_str(), 0, 0, &bx, &by, &bw, &bh);
                display.setCursor((W - (int)bw) / 2, lineY);
                display.print(line);
                lineY += lineH;
                start = i + 1;
            }
        }
    } while (display.nextPage());
}

// WMO weather code → short description
const char* wmoDesc(int code) {
    if (code == 0)              return "Clear Sky";
    if (code == 1)              return "Mainly Clear";
    if (code == 2)              return "Partly Cloudy";
    if (code == 3)              return "Overcast";
    if (code <= 48)             return "Foggy";
    if (code <= 55)             return "Drizzle";
    if (code <= 65)             return "Rain";
    if (code <= 77)             return "Snow";
    if (code <= 82)             return "Rain Showers";
    if (code <= 86)             return "Snow Showers";
    if (code >= 95)             return "Thunderstorm";
    return "Unknown";
}

// =============================================================================
// LUNAR CALENDAR — Vietnamese Am Lich (algorithm by Ho Ngoc Duc)
// =============================================================================
static long jdFromDate(int dd, int mm, int yyyy) {
    long a = (14 - mm) / 12;
    long y = yyyy + 4800 - a;
    long m = mm + 12 * a - 3;
    return dd + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32045;
}
static long getNewMoonDay(int k, float tz) {
    const double dr = M_PI / 180.0;
    double T  = k / 1236.85, T2 = T*T, T3 = T2*T;
    double Jd1 = 2415020.75933 + 29.53058868*k + 0.0001178*T2 - 0.000000155*T3;
    Jd1 += 0.00033 * sin((166.56 + 132.87*T - 0.009173*T2) * dr);
    double M   = 357.52910 + 29.10535608*k - 0.0000333*T2 - 0.00000347*T3;
    double Mpr = 306.0253 + 385.81691806*k + 0.0107306*T2 + 0.00001236*T3;
    double F   = 21.2964 + 390.67050646*k - 0.0016528*T2 - 0.00000239*T3;
    double C1  = (0.1734 - 0.000393*T)*sin(M*dr) + 0.0021*sin(2*dr*M)
               - 0.4068*sin(Mpr*dr) + 0.0161*sin(2*dr*Mpr) - 0.0004*sin(3*dr*Mpr)
               + 0.0104*sin(2*dr*F)  - 0.0051*sin((M+Mpr)*dr) - 0.0074*sin((M-Mpr)*dr)
               + 0.0004*sin((2*F+M)*dr) - 0.0004*sin((2*F-M)*dr) - 0.0006*sin((2*F+Mpr)*dr)
               + 0.0010*sin((2*F-Mpr)*dr) + 0.0005*sin((M+2*Mpr)*dr);
    double dt = (T < -11) ? 0.001 + 0.000839*T + 0.0002261*T2 - 0.00000845*T3 - 0.000000081*T*T3
                           : -0.000278 + 0.000265*T + 0.000262*T2;
    return (long)(Jd1 + C1 - dt + 0.5 + tz/24.0);
}
static int getSunLongitude(long jdn, float tz) {
    const double dr = M_PI / 180.0;
    double T = (jdn - 2451545.5 - tz/24.0) / 36525.0, T2 = T*T;
    double M  = 357.52910 + 35999.05030*T - 0.0001559*T2 - 0.00000048*T*T2;
    double L0 = 280.46645 + 36000.76983*T + 0.0003032*T2;
    double DL = (1.9146 - 0.004817*T - 0.000014*T2)*sin(dr*M)
              + (0.019993 - 0.000101*T)*sin(dr*2*M) + 0.00029*sin(dr*3*M);
    double L  = (L0 + DL - 0.00569 - 0.00478*sin((125.04 - 1934.136*T)*dr)) * dr;
    L -= M_PI*2 * (int)(L / (M_PI*2));
    if (L < 0) L += M_PI*2;
    return (int)(L / M_PI * 6);
}
static long getLeapMonthOffset(long a11, float tz) {
    int k = (int)((a11 - 2415021.076998695) / 29.530588853 + 0.5);
    int last = 0, i = 1;
    int arc = getSunLongitude(getNewMoonDay(k+i, tz), tz);
    do { last = arc; i++; arc = getSunLongitude(getNewMoonDay(k+i, tz), tz); } while (arc != last && i < 14);
    return i - 1;
}
struct LunarDate { int day, month, year; bool leap; };
static LunarDate solarToLunar(int dd, int mm, int yyyy) {
    const float TZ = 7.0f;
    long dayNum = jdFromDate(dd, mm, yyyy);
    int  k   = (int)((dayNum - 2415021.076998695) / 29.530588853);
    long mon = getNewMoonDay(k+1, TZ);
    if (mon > dayNum) mon = getNewMoonDay(k, TZ);
    long a11_cur = getNewMoonDay((int)((jdFromDate(31,12,yyyy)   - 2415021.076998695) / 29.530588853), TZ);
    long a11, b11; int yy;
    if (a11_cur >= mon) {
        yy  = yyyy;
        a11 = getNewMoonDay((int)((jdFromDate(31,12,yyyy-1) - 2415021.076998695) / 29.530588853), TZ);
        b11 = a11_cur;
    } else {
        yy  = yyyy + 1;
        a11 = a11_cur;
        b11 = getNewMoonDay((int)((jdFromDate(31,12,yyyy+1) - 2415021.076998695) / 29.530588853), TZ);
    }
    int  lunarDay = (int)(dayNum - mon) + 1;
    long diff     = (mon - a11) / 29;
    bool leapMon  = false;
    int  lunarMon = (int)diff + 11;
    if (b11 - a11 > 365) {
        long lo = getLeapMonthOffset(a11, TZ);
        if (diff >= lo) { lunarMon = (int)diff + 10; if (diff == lo) leapMon = true; }
    }
    if (lunarMon > 12) lunarMon -= 12;
    if (lunarMon >= 11 && diff < 4) yy--;
    return { lunarDay, lunarMon, yy, leapMon };
}
static const char* canChiYear(int year) {
    static char buf[16];
    // Earthly Branches mapped to English zodiac animals (same index as chi array)
    const char* animal[] = {"Monkey","Rooster","Dog","Pig","Rat","Ox","Tiger","Rabbit","Dragon","Snake","Horse","Goat"};
    snprintf(buf, sizeof(buf), "Yr. %s", animal[year % 12]);
    return buf;
}
static const char* lunarMonthName(int m) {
    const char* names[] = {"","Month 1","Month 2","Month 3","Month 4","Month 5","Month 6",
                            "Month 7","Month 8","Month 9","Month 10","Month 11","Month 12"};
    return (m >= 1 && m <= 12) ? names[m] : "---";
}

// =============================================================================
// DRAW SCREEN PORTRAIT (rotation 1=90° or 3=270°) — 384×640
// =============================================================================
void drawScreenPortrait() {
    const int W = 384, H = 640;

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timeBuf[10];
    char dateBuf[32];
    if (timeinfo->tm_year < 100) {
        strcpy(timeBuf, "--:--");
        strcpy(dateBuf, "Syncing...");
    } else {
        strftime(timeBuf, 10, "%H:%M", timeinfo);
        strftime(dateBuf, 32, "%A, %d/%m/%Y", timeinfo);
    }

    display.firstPage();
    do {
        Serial.println("[Draw-P] Paging...");
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        int16_t tbx, tby; uint16_t tbw, tbh;

        // ── TIME ─────────────────────────────────────
        display.setTextColor(GxEPD_RED);
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(2);
        display.getTextBounds(timeBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((W - tbw) / 2, 78);
        display.print(timeBuf);
        display.setTextSize(1);

        // ── DATE ─────────────────────────────────────
        display.setFont(&FreeSansBold12pt7b);
        display.getTextBounds(dateBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((W - tbw) / 2, 103);
        display.print(dateBuf);
        display.setTextColor(GxEPD_BLACK);

        // ── DIVIDER ────────────────────────────────
        display.drawLine(10, 116, W - 10, 116, GxEPD_BLACK);

        // ── LICH AM (left 1/3) ──────────────────── y: 116-215
        {
            int cx = W / 6;
            LunarDate lunar = solarToLunar(timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);

            display.setFont(&FreeSansBold9pt7b);
            display.getTextBounds("LUNAR", 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 129);
            display.print("LUNAR");

            display.setFont(&FreeSansBold18pt7b);
            char lunarDayBuf[10];
            snprintf(lunarDayBuf, sizeof(lunarDayBuf), "%d", lunar.day);
            display.getTextBounds(lunarDayBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 162);
            display.print(lunarDayBuf);

            display.setFont(&FreeSansBold9pt7b);
            char monBuf[12];
            if (lunar.leap)
                snprintf(monBuf, sizeof(monBuf), "%s*", lunarMonthName(lunar.month));
            else
                snprintf(monBuf, sizeof(monBuf), "%s", lunarMonthName(lunar.month));
            display.getTextBounds(monBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 180);
            display.print(monBuf);

            const char* cc = canChiYear(lunar.year);
            display.getTextBounds(cc, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 198);
            display.print(cc);
        }

        // ── WEATHER icon+desc (center 1/3) ──────── y: 116-215
        {
            int cx = W / 2;
            if (weather.valid) {
                display.drawBitmap(cx - 32, 120, getIcon(weather.currentCode), 64, 64, GxEPD_BLACK);
                display.setFont(&FreeSansBold9pt7b);
                const char* desc = wmoDesc(weather.currentCode);
                display.getTextBounds(desc, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, 202);
                display.print(desc);
            } else {
                display.setFont(&FreeSansBold9pt7b);
                const char* nd = "No Weather";
                display.getTextBounds(nd, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, 165);
                display.print(nd);
            }
        }

        // ── OUTDOOR (right 1/3) ───────────────── y: 116-215
        if (weather.valid) {
            int cx = W * 5 / 6;
            display.setFont(&FreeSansBold9pt7b);
            display.getTextBounds("OUTDOOR", 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 131);
            display.print("OUTDOOR");

            display.setFont(&FreeSansBold18pt7b);
            String outTemp = String((int)round(weather.currentTemp));
            outTemp += (char)176; outTemp += "C";
            display.getTextBounds(outTemp.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 170);
            display.print(outTemp);

            display.setFont(&FreeSansBold9pt7b);
            String rng = String((int)weather.daily[0].maxTemp) + "/" + String((int)weather.daily[0].minTemp);
            rng += (char)176; rng += "C";
            display.getTextBounds(rng.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, 194);
            display.print(rng);
        }

        // ── DIVIDERS ───────────────────────────── y: 215
        display.drawLine(10, 215, W - 10, 215, GxEPD_BLACK);
        display.drawLine(W/3,   116, W/3,   215, GxEPD_BLACK);
        display.drawLine(2*W/3, 116, 2*W/3, 215, GxEPD_BLACK);

        // ── FORECAST 5 rows ────────────────────── y: 220-640
        if (weather.valid) {
            int count = min(weather.dailyCount, 5);
            int rowH  = (H - 220) / count; // ~84px per row
            for (int i = 0; i < count; i++) {
                int y0   = 220 + i * rowH;
                int yMid = y0 + rowH / 2;
                if (i > 0) display.drawLine(10, y0, W - 10, y0, GxEPD_BLACK);

                // Day name — left
                char dayName[12];
                struct tm* dtm = localtime(&weather.daily[i].date);
                strftime(dayName, 12, "%A", dtm);
                display.setFont(&FreeSansBold12pt7b);
                display.getTextBounds(dayName, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(12, yMid + tbh/2);
                display.print(dayName);

                // Icon — center (only if row tall enough)
                if (rowH >= 66)
                    display.drawBitmap(W/2 - 32, y0 + (rowH - 64)/2, getIcon(weather.daily[i].code), 64, 64, GxEPD_BLACK);

                // Temp — right
                String t = String((int)weather.daily[i].maxTemp) + "/" + String((int)weather.daily[i].minTemp);
                t += (char)176; t += "C";
                display.setFont(&FreeSansBold9pt7b);
                display.getTextBounds(t.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(W - tbw - 12, yMid + tbh/2);
                display.print(t);
            }
        }
    } while (display.nextPage());
    display.powerOff();
}

// =============================================================================
// DRAW SCREEN (Clock + Sensor + Weather)
// =============================================================================
void drawScreen() {
    Serial.printf("[Draw] Weather Valid: %d\n", weather.valid);

    display.setFullWindow();

    if (showImageMode) {
        drawTestImage();
        return;
    }

    // Portrait layout for 90° / 270°
    if (config_rotation == 1 || config_rotation == 3) {
        drawScreenPortrait();
        return;
    }

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timeBuf[10];
    char dateBuf[32];

    if (timeinfo->tm_year < 100) {
        strcpy(timeBuf, "--:--");
        strcpy(dateBuf, "Syncing...");
    } else {
        strftime(timeBuf, 10, "%H:%M", timeinfo);
        strftime(dateBuf, 32, "%A, %d/%m/%Y", timeinfo);
    }

    display.firstPage();
    do {
        Serial.println("[Draw] Paging...");
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        int16_t tbx, tby; uint16_t tbw, tbh;
        const int Y = 30; // offset toàn bộ nội dung xuống N px — đổi số này để dịch chuyển ─────────────────────────────── y: 0-88
        display.setTextColor(GxEPD_RED);
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(2);
        display.getTextBounds(timeBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((640 - tbw) / 2, Y + 60);
        display.print(timeBuf);
        display.setTextSize(1);

        // ── DATE ──────────────────────────────── y: 88-108
        display.setFont(&FreeSansBold12pt7b);
        display.getTextBounds(dateBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((640 - tbw) / 2, Y + 96);
        display.print(dateBuf);
        display.setTextColor(GxEPD_BLACK);

        // ── DIVIDER ───────────────────────────── y: 112
        display.drawLine(10, Y + 112, 630, Y + 112, GxEPD_BLACK);

        // ── LICH AM (left 1/3: x 0-213) ── y: 118-240
        {
            const int cx = 106;
            LunarDate lunar = solarToLunar(timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);

            // Label
            display.setFont(&FreeSansBold9pt7b);
            const char* lbl = "LUNAR";
            display.getTextBounds(lbl, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 130);
            display.print(lbl);

            // Ngay / Thang am lich — font lon
            display.setFont(&FreeSansBold18pt7b);
            char lunarDayBuf[10];
            snprintf(lunarDayBuf, sizeof(lunarDayBuf), "%d", lunar.day);
            display.getTextBounds(lunarDayBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 168);
            display.print(lunarDayBuf);

            // Ten thang am
            display.setFont(&FreeSansBold9pt7b);
            char monBuf[12];
            if (lunar.leap)
                snprintf(monBuf, sizeof(monBuf), "%s*", lunarMonthName(lunar.month));
            else
                snprintf(monBuf, sizeof(monBuf), "%s", lunarMonthName(lunar.month));
            display.getTextBounds(monBuf, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 188);
            display.print(monBuf);

            // Nam Can Chi
            const char* cc = canChiYear(lunar.year);
            display.getTextBounds(cc, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 207);
            display.print(cc);
        }

        // ── WEATHER ICON + DESC (center 1/3: x 213-426) ── y: 118-240
        {
            int cx = 320;
            if (weather.valid) {
                display.drawBitmap(cx - 32, Y + 120, getIcon(weather.currentCode), 64, 64, GxEPD_BLACK);
                display.setFont(&FreeSansBold9pt7b);
                const char* desc = wmoDesc(weather.currentCode);
                display.getTextBounds(desc, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, Y + 202);
                display.print(desc);
            } else {
                display.setFont(&FreeSansBold9pt7b);
                const char* nd = "No Weather Data";
                display.getTextBounds(nd, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, Y + 160);
                display.print(nd);
            }
        }

        // ── OUTDOOR WEATHER TEMP (right 1/3: x 426-640) ── y: 118-240
        if (weather.valid) {
            int cx = 533;
            display.setFont(&FreeSansBold9pt7b);
            const char* outLabel = "OUTDOOR";
            display.getTextBounds(outLabel, 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 132);
            display.print(outLabel);

            display.setFont(&FreeSansBold18pt7b);
            String outTemp = String((int)round(weather.currentTemp));
            outTemp += (char)176;
            outTemp += "*C";
            display.getTextBounds(outTemp.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 175);
            display.print(outTemp);

            // Max / Min from today
            display.setFont(&FreeSansBold9pt7b);
            String rng = String((int)weather.daily[0].maxTemp) + "/" + String((int)weather.daily[0].minTemp);
            rng += (char)176;
            rng += "C";
            display.getTextBounds(rng.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            display.setCursor(cx - tbw/2, Y + 200);
            display.print(rng);
        }

        // ── DIVIDER ───────────────────────────── y: 215
        display.drawLine(10, Y + 215, 630, Y + 215, GxEPD_BLACK);
        // vertical separators
        display.drawLine(213, Y + 112, 213, Y + 215, GxEPD_BLACK);
        display.drawLine(426, Y + 112, 426, Y + 215, GxEPD_BLACK);

        // ── FORECAST (5 days) ─────────────────── y: 220-384
        if (weather.valid) {
            int count   = min(weather.dailyCount, 5);
            int colW    = 640 / count;
            int iconY   = Y + 240;
            int dayY    = Y + 233;
            int tempY   = Y + 320;

            for (int i = 0; i < count; i++) {
                int cx = colW * i + colW / 2;

                // vertical separator
                if (i > 0) display.drawLine(colW * i, Y + 220, colW * i, 384, GxEPD_BLACK);

                char dayName[10];
                struct tm* dtm = localtime(&weather.daily[i].date);
                strftime(dayName, 10, "%a", dtm);

                display.setFont(&FreeSansBold9pt7b);
                display.getTextBounds(dayName, 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, dayY);
                display.print(dayName);

                display.drawBitmap(cx - 32, iconY, getIcon(weather.daily[i].code), 64, 64, GxEPD_BLACK);

                String t = String((int)weather.daily[i].maxTemp) + "/" + String((int)weather.daily[i].minTemp);
                display.setFont(&FreeSansBold9pt7b);
                display.getTextBounds(t.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
                display.setCursor(cx - tbw/2, tempY);
                display.print(t);
            }
        }
    } while (display.nextPage());
    display.powerOff();
}

// =============================================================================
// FETCH WEATHER
// =============================================================================
void fetchWeather() {
    Serial.println("\n[Weather] Updating...");
    Serial.printf("[Weather] Free Heap: %d\n", ESP.getFreeHeap());
    weather.valid = false;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] WiFi Not Connected");
        return;
    }

    // Dùng plain HTTP (không HTTPS) — ESP32-C3 gặp lỗi TLS -1 với WiFiClientSecure
    // do thiếu root CA. Open-meteo hỗ trợ HTTP không cần TLS.
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(15000);
    http.setConnectTimeout(10000);

    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(config_lat) +
                 "&longitude=" + String(config_lon) +
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                 "&current=temperature_2m,weather_code"
                 "&timezone=Asia%2FBangkok&forecast_days=5";

    Serial.println("[Weather] URL: " + url);

    if (!http.begin(client, url)) {
        Serial.println("[Weather] http.begin Failed");
        return;
    }

    int httpCode = http.GET();
    Serial.printf("[Weather] HTTP Code: %d\n", httpCode);

    if (httpCode != 200) {
        Serial.println("[Weather] Error: " + String(httpCode));
        if (httpCode > 0) Serial.println(http.getString().substring(0, 200));
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();
    Serial.printf("[Weather] Payload: %d bytes\n", payload.length());
    if (payload.length() > 0) {
        Serial.println("[Weather] Start: " + payload.substring(0, 120));
    } else {
        Serial.println("[Weather] Empty payload!");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[Weather] JSON err: %s\n", err.c_str());
        return;
    }

    if (!doc["current"].is<JsonObject>()) {
        Serial.println("[Weather] No 'current' object in JSON");
        return;
    }
    weather.currentTemp = doc["current"]["temperature_2m"].as<float>();
    weather.currentCode = doc["current"]["weather_code"].as<int>();
    Serial.printf("[Weather] Current: %.1fC code=%d\n", weather.currentTemp, weather.currentCode);

    JsonArray codeArr = doc["daily"]["weather_code"].as<JsonArray>();
    JsonArray maxArr  = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    JsonArray minArr  = doc["daily"]["temperature_2m_min"].as<JsonArray>();

    if (!codeArr || !maxArr || !minArr) {
        Serial.println("[Weather] Missing daily arrays");
        return;
    }

    time_t nowTs = time(nullptr);
    int count = min(5, (int)codeArr.size());
    for (int i = 0; i < count; i++) {
        weather.daily[i].date    = nowTs + (i * 86400);
        weather.daily[i].code    = codeArr[i].as<int>();
        weather.daily[i].maxTemp = maxArr[i].as<float>();
        weather.daily[i].minTemp = minArr[i].as<float>();
    }
    weather.dailyCount = count;
    weather.valid = true;
    Serial.printf("[Weather] OK — %d days, Heap: %d\n", count, ESP.getFreeHeap());
}

// =============================================================================
// OTA UPDATE
// =============================================================================
void performOTA() {
    showMessage("OTA UPDATE", "Keep Power On!");
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000);

    t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_URL);
    if (ret == HTTP_UPDATE_FAILED) {
        showMessage("OTA FAIL", httpUpdate.getLastErrorString());
        delay(3000);
    }
    ESP.restart();
}

// =============================================================================
// ENTER WIFI CONFIG MODE
// =============================================================================
void enterConfigMode() {
    showMessage("WIFI SETUP", "AP: BECUBE-CLOCK\n192.168.4.1");

    // Clean up any running server / WiFi state
    server.stop();
    WiFi.disconnect(true);
    delay(300);

    WiFiManager wm;
    wm.setCustomHeadElement(WM_HEAD_EXTRA);
    // Pin the portal IP so 192.168.4.1 always works
    wm.setAPStaticIPConfig(
        IPAddress(192, 168, 4, 1),
        IPAddress(192, 168, 4, 1),
        IPAddress(255, 255, 255, 0));
    // No timeout — stay open until user saves config
    wm.setConfigPortalTimeout(0);

    // Re-add custom params so they survive a portal re-save
    char rotDegStr[4];
    snprintf(rotDegStr, sizeof(rotDegStr), "%d", rotIdxToDeg(config_rotation));
    WiFiManagerParameter p_lat("lat", "Latitude (Vi do)", config_lat, 16);
    WiFiManagerParameter p_lon("lon", "Longitude (Kinh do)", config_lon, 16);
    WiFiManagerParameter p_rot("rot", "Rotation (0/90/180/270)", rotDegStr, 4);
    wm.addParameter(&p_lat);
    wm.addParameter(&p_lon);
    wm.addParameter(&p_rot);

    // Task FreeRTOS theo dõi 5x press trong khi portal blocking
    TaskHandle_t wmBtnTask2 = nullptr;
    xTaskCreate(btnDeepSleepTask, "wm_btn2", 1024, nullptr, 1, &wmBtnTask2);

    bool saved = wm.startConfigPortal("BECUBE-CLOCK");

    if (wmBtnTask2) { vTaskDelete(wmBtnTask2); wmBtnTask2 = nullptr; }

    if (saved) {
        strcpy(config_lat, p_lat.getValue());
        strcpy(config_lon, p_lon.getValue());
        config_rotation = rotDegToIdx(atoi(p_rot.getValue()));
        saveConfig();
    }
    ESP.restart();
}

// =============================================================================
// DRAW TEST IMAGE (fallback nếu không có custom.bin)
// =============================================================================
void drawTestImage() {
    if (countImages() > 0) {
        handleImageUpload();
        return;
    }

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        int w = display.width(), h = display.height();

        display.drawRect(5, 5, w - 10, h - 10, GxEPD_RED);
        display.drawRect(7, 7, w - 14, h - 14, GxEPD_BLACK);
        display.fillCircle(w / 2, h / 2, 120, GxEPD_RED);
        display.fillCircle(w / 2, h / 2, 80, GxEPD_BLACK);
        display.fillCircle(w / 2, h / 2, 40, GxEPD_WHITE);

        display.setFont(&FreeSansBold24pt7b);
        int16_t tbx, tby; uint16_t tbw, tbh;
        const char* text = "ESP32-C3 TEST";
        display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((w - tbw) / 2, 60);
        display.setTextColor(GxEPD_RED);
        display.print(text);

        int barW = w / 3, barH = 40, barY = h - 60;
        display.fillRect(0,        barY, barW, barH, GxEPD_BLACK);
        display.fillRect(barW,     barY, barW, barH, GxEPD_RED);
        display.fillRect(barW * 2, barY, barW, barH, GxEPD_WHITE);
        display.drawRect(barW * 2, barY, barW, barH, GxEPD_BLACK);
    } while (display.nextPage());
    display.powerOff();
}

// =============================================================================
// HANDLE IMAGE UPLOAD - ESP32 OPTIMIZED (full image in RAM)
// =============================================================================
void handleImageUpload() {
    Serial.println("[Image] Drawing uploaded image...");

    String imgFilePath = imgPath(currentImageIndex);
    if (!LittleFS.exists(imgFilePath)) {
        Serial.printf("[Image] Error: %s not found\n", imgFilePath.c_str());
        return;
    }

    File f = LittleFS.open(imgFilePath, "r");
    if (!f) {
        Serial.println("[Image] Error: Cannot open file");
        return;
    }

    uint32_t fileSize = f.size();
    // Always use native physical panel dimensions (640×384)
    // Web JS pre-rotates pixel data for portrait modes
    const uint16_t NATIVE_W = 640, NATIVE_H = 384;
    uint32_t standardSize = ((uint32_t)NATIVE_W * NATIVE_H) / 8; // 30720 bytes per plane
    Serial.printf("[Image] File Size: %d bytes, native: %dx%d\n", fileSize, NATIVE_W, NATIVE_H);

    if (fileSize < standardSize) {
        Serial.println("[Image] Error: File too small");
        f.close();
        return;
    }

    // --- ESP32 ADVANTAGE: Load entire image into RAM ---
    uint8_t* bufBW  = (uint8_t*)malloc(standardSize);
    uint8_t* bufRed = (uint8_t*)malloc(standardSize);

    if (!bufBW || !bufRed) {
        Serial.printf("[Image] malloc failed! Heap: %d\n", ESP.getFreeHeap());
        if (bufBW)  free(bufBW);
        if (bufRed) free(bufRed);
        f.close();
        return;
    }

    // Read BW plane
    f.seek(0, SeekSet);
    f.read(bufBW, standardSize);

    // Read RED plane (if present)
    if (fileSize >= standardSize * 2) {
        f.seek(standardSize, SeekSet);
        f.read(bufRed, standardSize);
    } else {
        memset(bufRed, 0x00, standardSize);
    }
    f.close();

    Serial.println("[Image] Buffers loaded. Drawing...");

    // writeImage bypasses setRotation and writes directly to the physical panel
    // buffer. JS pre-rotates data for portrait modes (1/3). For 180° (rotation=2),
    // we must flip the buffer 180° in-place so the image matches clock orientation.
    if (config_rotation == 2) {
        auto rev8 = [](uint8_t b) -> uint8_t {
            b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
            b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
            b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
            return b;
        };
        const uint16_t rowB = NATIVE_W / 8; // 80 bytes per row
        for (int y = 0; y < NATIVE_H / 2; y++) {
            uint8_t *r1 = bufBW  + y * rowB, *r2 = bufBW  + (NATIVE_H - 1 - y) * rowB;
            uint8_t *s1 = bufRed + y * rowB, *s2 = bufRed + (NATIVE_H - 1 - y) * rowB;
            for (int x = 0; x < rowB; x++) {
                uint8_t b1 = r1[x], b2 = r2[rowB - 1 - x];
                r1[x] = rev8(b2);  r2[rowB - 1 - x] = rev8(b1);
                b1 = s1[x];        b2 = s2[rowB - 1 - x];
                s1[x] = rev8(b2);  s2[rowB - 1 - x] = rev8(b1);
            }
        }
    }

    // For portrait rotations (1/3), temporarily set rotation=0 so writeImage
    // fills the full physical panel correctly with the pre-rotated data.
    bool isPortrait = (config_rotation == 1 || config_rotation == 3);
    if (isPortrait) display.setRotation(0);

    display.writeImage(bufBW, bufRed, 0, 0, NATIVE_W, NATIVE_H, false, false, false);
    display.refresh(false); // full refresh

    if (isPortrait) display.setRotation(config_rotation); // restore

    free(bufBW);
    free(bufRed);
    display.powerOff();
    Serial.println("[Image] Draw Complete");
}

// =============================================================================
// IMAGE SERVER (AP Mode + Web Upload)
// =============================================================================
void startImageServer() {
    Serial.println("[ImageServer] Starting...");

    {
        int W = display.width();
        int H = display.height();
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            int16_t bx, by; uint16_t bw, bh;

            display.setFont(&FreeSansBold24pt7b);
            const char* msg = "IMAGE MODE";
            display.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
            display.setCursor((W - (int)bw) / 2, H / 2 - 50);
            display.print(msg);

            display.setFont(&FreeSansBold12pt7b);
            const char* lines[] = {"WiFi: BECUBE-IMG", "(No Password)", "http://192.168.4.1"};
            int yPos = H / 2 + 10;
            for (int i = 0; i < 3; i++) {
                display.getTextBounds(lines[i], 0, 0, &bx, &by, &bw, &bh);
                display.setCursor((W - (int)bw) / 2, yPos);
                display.print(lines[i]);
                yPos += 32;
            }
        } while (display.nextPage());
    }

    // Start AP
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(300);

    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    const char* ssid = "BECUBE-IMG";
    bool apOk = WiFi.softAP(ssid);
    delay(1000);

    if (apOk) {
        Serial.printf("[ImageServer] AP '%s' ready. IP: %s\n", ssid, WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[ImageServer] AP Failed!");
        return;
    }

    Serial.printf("[ImageServer] LittleFS: Total=%d, Used=%d\n",
        LittleFS.totalBytes(), LittleFS.usedBytes());

    // --- ROUTES ---
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    server.on("/upload_chunk", HTTP_POST,
        []() {
            server.send(200, "text/plain", "Chunk Received");
        },
        []() {
            HTTPUpload& upload = server.upload();

            if (upload.status == UPLOAD_FILE_START) {
                int offset = server.arg("offset").toInt();
                Serial.printf("[Upload] Chunk Start: offset=%d\n", offset);
                if (offset == 0) {
                    // New upload session — always start with a clean file
                    if (LittleFS.exists("/tmp.bin")) LittleFS.remove("/tmp.bin");
                    uploadFile = LittleFS.open("/tmp.bin", "w");
                } else {
                    uploadFile = LittleFS.open("/tmp.bin", "a");
                }
                if (!uploadFile) Serial.println("[Upload] File Open Fail");
                else Serial.printf("[Upload] File opened OK, size before write=%d\n",
                    LittleFS.open("/tmp.bin", "r").size());

            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);

            } else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.printf("[Upload] Chunk End. Written: %d bytes\n", upload.totalSize);
                }
            }
        }
    );

    server.on("/upload_finish", HTTP_POST, []() {
        if (!LittleFS.exists("/tmp.bin")) {
            Serial.println("[Upload] Finish: /tmp.bin missing!");
            server.send(500, "text/plain", "Upload data lost");
            return;
        }
        File check = LittleFS.open("/tmp.bin", "r");
        uint32_t fSize = check ? check.size() : 0;
        if (check) check.close();
        Serial.printf("[Upload] Finish: /tmp.bin size=%d\n", fSize);
        // Find next empty slot; if all 5 full, overwrite slot 0
        int slot = -1;
        for (int i = 0; i < MAX_IMAGES; i++) {
            if (!LittleFS.exists(imgPath(i))) { slot = i; break; }
        }
        if (slot == -1) slot = 0;
        if (LittleFS.exists(imgPath(slot))) LittleFS.remove(imgPath(slot));
        bool ok = LittleFS.rename("/tmp.bin", imgPath(slot));
        if (!ok) {
            Serial.printf("[Upload] Rename failed! slot=%d\n", slot);
            if (LittleFS.exists("/tmp.bin")) LittleFS.remove("/tmp.bin");
            server.send(500, "text/plain", "File rename failed");
            return;
        }
        // Guard: ensure no stale /tmp.bin remains
        if (LittleFS.exists("/tmp.bin")) LittleFS.remove("/tmp.bin");
        currentImageIndex = slot;
        int total = countImages();
        Serial.printf("[Upload] Saved to slot %d, total=%d\n", slot, total);
        JsonDocument resp;
        resp["slot"] = slot;
        resp["total"] = total;
        String json;
        serializeJson(resp, json);
        server.send(200, "application/json", json);
        showImageMode = true;
        imageUpdatePending = true;
    });

    server.on("/images", HTTP_GET, []() {
        JsonDocument doc;
        doc["count"] = countImages();
        JsonArray arr = doc["images"].to<JsonArray>();
        for (int i = 0; i < MAX_IMAGES; i++) {
            if (LittleFS.exists(imgPath(i))) {
                JsonObject obj = arr.add<JsonObject>();
                obj["id"] = i;
                File f = LittleFS.open(imgPath(i), "r");
                obj["size"] = f ? (int)f.size() : 0;
                if (f) f.close();
            }
        }
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    server.on("/rotation", HTTP_GET, []() {
        JsonDocument doc;
        doc["rotation"] = config_rotation;
        doc["degrees"]  = rotIdxToDeg(config_rotation);
        doc["width"]    = display.width();
        doc["height"]   = display.height();
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    server.on("/delete", HTTP_POST, []() {
        if (!server.hasArg("id")) {
            server.send(400, "text/plain", "Missing id");
            return;
        }
        int id = server.arg("id").toInt();
        if (id < 0 || id >= MAX_IMAGES) {
            server.send(400, "text/plain", "Invalid id");
            return;
        }
        if (LittleFS.exists(imgPath(id))) {
            LittleFS.remove(imgPath(id));
            Serial.printf("[Image] Deleted slot %d\n", id);
        }
        server.send(200, "text/plain", "OK");
    });

    server.onNotFound([]() {
        server.send(404, "text/plain", "Not Found");
    });

    server.begin();
    Serial.println("[ImageServer] HTTP Server Started!");
}