// MeterMonitor ver2 — USB upload only, no OTA; IP shows for 5s on Wi‑Fi/HeaterMeter connect.
// NOTE: Update your Wi‑Fi credentials below before uploading.
#include <WiFiManager.h>
#include <HeaterMeterClient.h>
#include <TM1637Display.h>

// ---- WiFi / HeaterMeter ----
// CHANGE THESE to your Wi‑Fi network name (SSID) and password:
#define WIFI_SSID       "network"    // <-- set to your Wi‑Fi SSID
#define WIFI_PASSWORD   "password"   // <-- set to your Wi‑Fi password
#define HEATERMEATER_IP ""           // IP or leave blank to use discovery

// ---- LED hardware (ESP8266 pin names D1..D7) ----
#define LED_BRIGHTNESS  7  // 1 (low) - 7 (high)
#define DPIN_LED_CLK    D1
#define DPIN_LED_DISP0  D2
#define DPIN_LED_DISP1  D7
#define DPIN_LED_DISP2  D6
#define DPIN_LED_DISP3  D5

#ifndef TEMP_COUNT
#define TEMP_COUNT 4
#endif

// ---- TM1637 letter glyphs (SEG_* from TM1637Display.h) ----
#define TM1637_L (SEG_D | SEG_E | SEG_F)
#define TM1637_I (SEG_B | SEG_C)
#define TM1637_D (SEG_B | SEG_C | SEG_D | SEG_E | SEG_G)
#define TM1637_O (SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F)
#define TM1637_P (SEG_A | SEG_B | SEG_E | SEG_F | SEG_G)
#define TM1637_E (SEG_A | SEG_D | SEG_E | SEG_F | SEG_G)
#define TM1637_N (SEG_C | SEG_E | SEG_G)
#define TM1637_S (SEG_A | SEG_C | SEG_D | SEG_F | SEG_G)
#define TM1637_C (SEG_A | SEG_D | SEG_E | SEG_F)
#define TM1637_T (SEG_D | SEG_E | SEG_F | SEG_G)
#define TM1637_W (SEG_B | SEG_D | SEG_F)
#define TM1637_F (SEG_A | SEG_E | SEG_F | SEG_G)
#define TM1637_G (SEG_A | SEG_C | SEG_D | SEG_E | SEG_F)

// ---- Forward declarations ----
static void setupLeds(void);
static void displayTemps(void);
static void ledsShowDeviceIp(void);
static void ledsShowHeaterIp(void);
static void ledsShowDisconnected(void);
static void ledsShowNoWifi(void);

// ---- App objects / state ----
static HeaterMeterClient hm(HEATERMEATER_IP);

static TM1637Display led0(DPIN_LED_CLK, DPIN_LED_DISP0, 90);
static TM1637Display led1(DPIN_LED_CLK, DPIN_LED_DISP1, 90);
static TM1637Display led2(DPIN_LED_CLK, DPIN_LED_DISP2, 90);
static TM1637Display led3(DPIN_LED_CLK, DPIN_LED_DISP3, 90);
static TM1637Display* leds[TEMP_COUNT];

static WiFiManager wm;

static float   g_LastTemps[TEMP_COUNT] = {0};
static uint8_t g_HmTempsChanged = 0;
static err_t   g_LastClientError = 0;

// ---- IP display window ----
static uint32_t g_ShowIpUntilMs = 0;
static bool     g_ShowHeaterIp  = false; // false=device IP, true=HeaterMeter remote IP
#define SHOW_IP_MS 5000UL  // 5 seconds

// ---- Display helpers ----
static void ledsShowDeviceIp(void) {
  IPAddress ip = WiFi.localIP();
  uint32_t v = (uint32_t)ip;
  leds[0]->showNumberDec(v & 0xff, false, 4, 0);
  leds[1]->showNumberDec((v >> 8) & 0xff, false, 4, 0);
  leds[2]->showNumberDec((v >> 16) & 0xff, false, 4, 0);
  leds[3]->showNumberDec(v >> 24, false, 4, 0);
}

static void ledsShowHeaterIp(void) {
  uint32_t ip = (uint32_t)hm.getRemoteIP();
  leds[0]->showNumberDec(ip & 0xff, false, 4, 0);
  leds[1]->showNumberDec((ip >> 8) & 0xff, false, 4, 0);
  leds[2]->showNumberDec((ip >> 16) & 0xff, false, 4, 0);
  leds[3]->showNumberDec(ip >> 24, false, 4, 0);
}

static void ledsShowDisconnected(void) {
  if (g_LastClientError != 0) leds[0]->showNumberDec(g_LastClientError);
  else leds[0]->clear();
  leds[1]->setSegments((const uint8_t[4]){ TM1637_D, TM1637_I, TM1637_S, TM1637_C });
  leds[2]->setSegments((const uint8_t[4]){ TM1637_O, TM1637_N, TM1637_N, TM1637_E });
  leds[3]->setSegments((const uint8_t[4]){ TM1637_C, TM1637_T, TM1637_E, TM1637_D });
}

static void ledsShowNoWifi(void) {
  leds[0]->clear();
  leds[1]->setSegments((const uint8_t[4]){ 0, TM1637_N, TM1637_O, 0 });
  leds[2]->setSegments((const uint8_t[4]){ TM1637_W, TM1637_I, TM1637_F, TM1637_I });
  leds[3]->clear();
}

static void setupLeds(void) {
  leds[0] = &led0; leds[1] = &led1; leds[2] = &led2; leds[3] = &led3;
  for (uint8_t i = 0; i < TEMP_COUNT; ++i) {
    leds[i]->setBrightness(LED_BRIGHTNESS);
    leds[i]->clear();
    yield();
  }
  ledsShowNoWifi(); // default screen while connecting
}

// Smooth temperature display, with LID overlays
static void displayTemps(void) {
  --g_HmTempsChanged;

  bool isLid = hm.state.LidCountdown > 0;
  for (uint8_t i = 0; i < TEMP_COUNT; ++i) {
    if (g_HmTempsChanged == 0) g_LastTemps[i] = hm.state.Probes[i].Temperature;

    if (isLid && i == 1) {
      leds[1]->setSegments((const uint8_t[4]){ TM1637_L, TM1637_I, TM1637_D, 0 });
    } else if (isLid && i == 2) {
      leds[2]->setSegments((const uint8_t[4]){ TM1637_O, TM1637_P, TM1637_E, TM1637_N });
    } else if (isLid && i == 3) {
      leds[3]->showNumberDecEx(hm.state.LidCountdown, 0);
    } else if (hm.state.Probes[i].HasTemperature) {
      float t = g_LastTemps[i] + ((4 - g_HmTempsChanged) / 4.0f) *
                (hm.state.Probes[i].Temperature - g_LastTemps[i]);
      leds[i]->showNumberDecEx((int32_t)(t * 10.0f), 0b00100000); // 1 decimal place
    } else {
      leds[i]->clear();
    }
    yield();
  }
}

// ---- HeaterMeter / WiFi callbacks ----
static void proxy_onHmStatus(void)      { g_HmTempsChanged = 4; }
static void proxy_onConnect(void)       { g_ShowHeaterIp = true;  g_ShowIpUntilMs = millis() + SHOW_IP_MS; ledsShowHeaterIp(); g_LastClientError = 0; }
static void proxy_onDisconnect(void)    { ledsShowDisconnected(); }
static void proxy_onError(err_t err)    {
  g_LastClientError = err;
  if (hm.getProtocolState() == HeaterMeterClient::hpsConnecting) ledsShowDisconnected();
#if defined(ESP8266)
  if (err == ERR_ABRT) ESP.restart();
#endif
}
static void proxy_onWifiConnect(void)   { g_ShowHeaterIp = false; g_ShowIpUntilMs = millis() + SHOW_IP_MS; ledsShowDeviceIp(); g_LastClientError = 0; }
static void proxy_onWifiDisconnect(void){ ledsShowNoWifi(); }

// ---- setup / loop ----
void setup() {
  Serial.begin(115200);

  hm.onHmStatus        = &proxy_onHmStatus;
  hm.onWifiConnect     = &proxy_onWifiConnect;
  hm.onWifiDisconnect  = &proxy_onWifiDisconnect;
  hm.onConnect         = &proxy_onConnect;
  hm.onDisconnect      = &proxy_onDisconnect;
  hm.onError           = &proxy_onError;

  setupLeds();

  // Option A: try hard-coded Wi-Fi first, then fall back to WiFiManager
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    proxy_onWifiConnect();
  } else {
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(120);
    // No AP callback → no "CON FIG" message on the displays
    wm.autoConnect("MeterMonitor");
  }
}

void loop() {
  wm.process();        // only active while the portal is open
  hm.update();

  // During the IP-show window, keep the chosen IP visible and skip temps
  if (g_ShowIpUntilMs != 0 && (int32_t)(millis() - g_ShowIpUntilMs) < 0) {
    if (g_ShowHeaterIp) ledsShowHeaterIp(); else ledsShowDeviceIp();
  } else {
    if (g_HmTempsChanged > 0) displayTemps();
  }

  if (hm.getProtocolState() != HeaterMeterClient::hpsNoNetwork)
    delay(25);
}
