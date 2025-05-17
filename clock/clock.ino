#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>

// Replace with your network credentials
const char* ssid = "Lahirufiber";
const char* password = "Lahiru@fiber7013";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600 + 30 * 60; // GMT+05:30 for Sri Lanka
const int daylightOffset_sec = 0;

// NTP UDP instance
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

// Variables for manual timekeeping
unsigned long currentTimeEpoch = 0;
unsigned long lastNtpUpdateTime = 0;
unsigned long lastUpdateTime = 0;
const long updateIntervalMs = 5000; // Update time from NTP every 5 seconds

// Variables for displaying time
unsigned long currentHours = 0;
unsigned long currentMinutes = 0;
unsigned long currentSeconds = 0;
int currentDayOfWeek = 0;
String daysOfTheWeek[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
unsigned long lastMinute = 0; // Track last minute for color change

// LED definitions
#define LED_PIN     13
#define NUM_LEDS    81
CRGB leds[NUM_LEDS];

// Define the segments
#define SECONDS_LEDS      60
#define MINUTES_LEDS      60
#define AMPM_LEDS         2
#define DAY_LEDS          7
#define HOUR_LEDS         12

// Dynamic colors (updated every minute)
CRGB secondColor = CRGB::Green;
CRGB minuteColor = CRGB::Blue;
CRGB ampmColor = CRGB::White;
CRGB dayColor = CRGB::Red;
CRGB hourColor = CRGB::Purple;
#define OFF_COLOR       CRGB::Black

// Predefined colors for cycling
const CRGB colorPalette[] = {
  CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Purple, CRGB::Orange,
  CRGB::Yellow, CRGB::Cyan, CRGB::Magenta, CRGB::Pink, CRGB::Lime,
  CRGB::Teal, CRGB::Violet
};
const int numColors = 12;
int colorCycleOffset = 0; // Tracks position in color palette

// Transition variables
#define TAIL_LENGTH 3 // Fixed tail length
#define SIDE_BRIGHTNESS 128 // Brightness for previous and next LEDs (0-255)
unsigned long lastSecondChange = 0;
float transitionProgress = 0.0;
const int transitionIntervalMs = 50; // Update every 50ms
const float transitionStep = transitionIntervalMs / 1000.0;

// Animation variables
#define ANIMATION_DURATION_MS 5000 // 5 seconds
#define ANIMATION_UPDATE_MS 50 // Update animation every 50ms
unsigned long animationStartTime = 0;
bool isAnimating = false;
unsigned long lastAnimationMinute = 0;

// Pattern control
int patternMode = 0; // 0: Original, 1: Minutes fill, 2: Seconds fill, 3: Rainbow seconds
const int numPatterns = 4; // Total number of patterns

// Update colors by cycling through the palette
void updateColors() {
  CRGB selectedColors[5];
  bool used[numColors] = {false};
  int selectedCount = 0;

  for (int i = 0; selectedCount < 5 && i < numColors; i++) {
    int index = (colorCycleOffset + i) % numColors;
    if (!used[index]) {
      selectedColors[selectedCount] = colorPalette[index];
      used[index] = true;
      selectedCount++;
    }
  }

  if (selectedCount < 5) {
    for (int i = 0; i < numColors && selectedCount < 5; i++) {
      if (!used[i]) {
        selectedColors[selectedCount] = colorPalette[i];
        used[i] = true;
        selectedCount++;
      }
    }
  }

  secondColor = selectedColors[0];
  minuteColor = selectedColors[1];
  ampmColor = selectedColors[2];
  dayColor = selectedColors[3];
  hourColor = selectedColors[4];

  colorCycleOffset = (colorCycleOffset + 1) % numColors;
  Serial.println("Colors cycled");
}

// Run ring animation
void runRingAnimation(unsigned long nowMs) {
  if (nowMs - animationStartTime >= ANIMATION_DURATION_MS) {
    isAnimating = false;
    patternMode = (patternMode + 1) % numPatterns; // Switch to next pattern
    Serial.print("Switched to pattern: ");
    Serial.println(patternMode);
    return;
  }

  FastLED.clear();

  static int animationStep = 0;
  if (nowMs - lastUpdateTime >= ANIMATION_UPDATE_MS) {
    animationStep = (animationStep + 1) % SECONDS_LEDS;
    lastUpdateTime = nowMs;
  }
  for (int i = 0; i < 3; i++) {
    int ledIndex = (animationStep + i) % SECONDS_LEDS;
    leds[ledIndex] = secondColor;
    leds[ledIndex].fadeToBlackBy(255 - (255 * (3 - i) / 3));
  }

  float pulse = (sin((nowMs - animationStartTime) / 1000.0 * PI) + 1) / 2;
  uint8_t brightness = (uint8_t)(255 * pulse);
  for (int i = 0; i < HOUR_LEDS; i++) {
    int ledIndex = SECONDS_LEDS + AMPM_LEDS + DAY_LEDS + i;
    leds[ledIndex] = hourColor;
    leds[ledIndex].fadeToBlackBy(255 - brightness);
  }

  unsigned long loopHours = currentHours;
  unsigned long finalHours = loopHours % 24;
  if (finalHours < 12) {
    leds[SECONDS_LEDS] = ampmColor;
    leds[SECONDS_LEDS + 1] = OFF_COLOR;
  } else {
    leds[SECONDS_LEDS] = OFF_COLOR;
    leds[SECONDS_LEDS + 1] = ampmColor;
  }
  int finalDayOfWeek = currentDayOfWeek + (loopHours / 24);
  if (finalDayOfWeek >= 7) finalDayOfWeek %= 7;
  leds[SECONDS_LEDS + AMPM_LEDS + (7 - finalDayOfWeek)] = dayColor;

  FastLED.show();
}

void connectWiFi() {
  Serial.print("Connecting to WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void getTimeFromNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    if (timeClient.update()) {
      currentTimeEpoch = timeClient.getEpochTime();
      lastNtpUpdateTime = millis();
      lastUpdateTime = millis();
      unsigned long rawTime = currentTimeEpoch;
      currentHours = (rawTime % 86400L) / 3600;
      currentMinutes = (rawTime % 3600) / 60;
      currentSeconds = rawTime % 60;
      currentDayOfWeek = timeClient.getDay();
      Serial.println("Time updated from NTP");
    } else {
      Serial.println("Failed to get time from NTP");
    }
  } else {
    Serial.println("WiFi not connected, cannot update time from NTP");
  }
}

void displayPattern0(unsigned long nowMs) {
  // Original pattern: Seconds with fading tail, minutes on single LED
  if (nowMs - lastUpdateTime >= transitionIntervalMs) {
    transitionProgress += transitionStep;
    if (transitionProgress > 1.0) transitionProgress = 1.0;
    lastUpdateTime = nowMs;

    FastLED.clear();

    // Seconds with symmetric tail
    int nextSecond = (currentSeconds + 1) % 60;
    int currentIndices[2] = {currentSeconds, (currentSeconds + 1) % SECONDS_LEDS};
    uint8_t currentBrightness[2] = {255 * (1.0 - transitionProgress), SIDE_BRIGHTNESS * (1.0 - transitionProgress)};
    int nextIndices[2] = {(currentSeconds + 1) % SECONDS_LEDS, (currentSeconds + 2) % SECONDS_LEDS};
    uint8_t nextBrightness[2] = {255 * transitionProgress, SIDE_BRIGHTNESS * transitionProgress};

    int brightnessSum[SECONDS_LEDS] = {0};
    for (int i = 0; i < 2; i++) {
      brightnessSum[currentIndices[i]] += currentBrightness[i];
      brightnessSum[nextIndices[i]] += nextBrightness[i];
    }
    for (int i = 0; i < SECONDS_LEDS; i++) {
      if (brightnessSum[i] > 0) {
        leds[i] = secondColor;
        leds[i].fadeToBlackBy(255 - min(brightnessSum[i], 255));
      }
    }

    // Minutes
    leds[currentMinutes] = minuteColor;

    // AM/PM, Day, Hours
    unsigned long loopHours = currentHours;
    unsigned long finalHours = loopHours % 24;
    if (finalHours < 12) {
      leds[SECONDS_LEDS] = ampmColor;
      leds[SECONDS_LEDS + 1] = OFF_COLOR;
    } else {
      leds[SECONDS_LEDS] = OFF_COLOR;
      leds[SECONDS_LEDS + 1] = ampmColor;
    }
    int finalDayOfWeek = currentDayOfWeek + (loopHours / 24);
    if (finalDayOfWeek >= 7) finalDayOfWeek %= 7;
    leds[SECONDS_LEDS + AMPM_LEDS + (7 - finalDayOfWeek)] = dayColor;
    int hour12 = finalHours % 12;
    if (hour12 == 0) hour12 = 12;
    leds[SECONDS_LEDS + AMPM_LEDS + DAY_LEDS + hour12] = hourColor;

    FastLED.show();
  }
}

void displayPattern1(unsigned long nowMs) {
  // Pattern 2: Minutes fill LEDs 0 to current minute, seconds on single LED
  if (nowMs - lastUpdateTime >= transitionIntervalMs) {
    lastUpdateTime = nowMs;
    FastLED.clear();

    // Minutes: Light up LEDs from 0 to currentMinutes
    for (int i = 0; i <= currentMinutes; i++) {
      leds[i] = minuteColor;
    }

    // Seconds: Single LED
    leds[currentSeconds] = secondColor; // Seconds overwrites minutes if same LED

    // AM/PM, Day, Hours
    unsigned long loopHours = currentHours;
    unsigned long finalHours = loopHours % 24;
    if (finalHours < 12) {
      leds[SECONDS_LEDS] = ampmColor;
      leds[SECONDS_LEDS + 1] = OFF_COLOR;
    } else {
      leds[SECONDS_LEDS] = OFF_COLOR;
      leds[SECONDS_LEDS + 1] = ampmColor;
    }
    int finalDayOfWeek = currentDayOfWeek + (loopHours / 24);
    if (finalDayOfWeek >= 7) finalDayOfWeek %= 7;
    leds[SECONDS_LEDS + AMPM_LEDS + (7 - finalDayOfWeek)] = dayColor;
    int hour12 = finalHours % 12;
    if (hour12 == 0) hour12 = 12;
    leds[SECONDS_LEDS + AMPM_LEDS + DAY_LEDS + hour12] = hourColor;

    FastLED.show();
  }
}

void displayPattern2(unsigned long nowMs) {
  // Pattern 3: Seconds fill LEDs 0 to current second, minutes on single LED
  if (nowMs - lastUpdateTime >= transitionIntervalMs) {
    lastUpdateTime = nowMs;
    FastLED.clear();

    // Seconds: Light up LEDs from 0 to currentSeconds
    for (int i = 0; i <= currentSeconds; i++) {
      leds[i] = secondColor;
    }

    // Minutes: Single LED
    leds[currentMinutes] = minuteColor; // Minutes overwrites seconds if same LED

    // AM/PM, Day, Hours
    unsigned long loopHours = currentHours;
    unsigned long finalHours = loopHours % 24;
    if (finalHours < 12) {
      leds[SECONDS_LEDS] = ampmColor;
      leds[SECONDS_LEDS + 1] = OFF_COLOR;
    } else {
      leds[SECONDS_LEDS] = OFF_COLOR;
      leds[SECONDS_LEDS + 1] = ampmColor;
    }
    int finalDayOfWeek = currentDayOfWeek + (loopHours / 24);
    if (finalDayOfWeek >= 7) finalDayOfWeek %= 7;
    leds[SECONDS_LEDS + AMPM_LEDS + (7 - finalDayOfWeek)] = dayColor;
    int hour12 = finalHours % 12;
    if (hour12 == 0) hour12 = 12;
    leds[SECONDS_LEDS + AMPM_LEDS + DAY_LEDS + hour12] = hourColor;

    FastLED.show();
  }
}

void displayPattern3(unsigned long nowMs) {
    // Pattern 4: Single minute LED, single second LED, all with changing rainbow colors
  if (nowMs - lastUpdateTime >= transitionIntervalMs) {
    lastUpdateTime = nowMs;
    FastLED.clear();

    // Calculate base hue based on currentSeconds (cycles every 60s)
    uint8_t baseHue = (uint8_t)(255 * currentSeconds / SECONDS_LEDS) % 255;

    // Minutes: Single LED with rainbow color
    leds[currentMinutes] = CHSV(baseHue, 255, 255); // Hue offset 0 for minutes

    // Seconds: Single LED with different rainbow color
    leds[currentSeconds] = CHSV((baseHue + 51) % 255, 255, 255); // Hue offset 51 for seconds

    // AM/PM, Day, Hours
    unsigned long loopHours = currentHours;
    unsigned long finalHours = loopHours % 24;
    if (finalHours < 12) {
      leds[SECONDS_LEDS] = CHSV((baseHue + 102) % 255, 255, 255); // Hue offset 102 for AM/PM
      leds[SECONDS_LEDS + 1] = OFF_COLOR;
    } else {
      leds[SECONDS_LEDS] = OFF_COLOR;
      leds[SECONDS_LEDS + 1] = CHSV((baseHue + 102) % 255, 255, 255);
    }
    int finalDayOfWeek = currentDayOfWeek + (loopHours / 24);
    if (finalDayOfWeek >= 7) finalDayOfWeek %= 7;
    leds[SECONDS_LEDS + AMPM_LEDS + (7 - finalDayOfWeek)] = CHSV((baseHue + 153) % 255, 255, 255); // Hue offset 153 for day
    int hour12 = finalHours % 12;
    if (hour12 == 0) hour12 = 12;
    leds[SECONDS_LEDS + AMPM_LEDS + DAY_LEDS + hour12] = CHSV((baseHue + 204) % 255, 255, 255); // Hue offset 204 for hours

    FastLED.show();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  connectWiFi();
  timeClient.begin();
  getTimeFromNTP();
  lastMinute = currentMinutes;
  lastAnimationMinute = currentMinutes;
  updateColors();
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
}

void loop() {
  unsigned long nowMs = millis();

  // Update seconds manually
  if (nowMs - lastSecondChange >= 1000) {
    currentSeconds = (currentSeconds + 1) % 60;
    if (currentSeconds == 0) {
      currentMinutes = (currentMinutes + 1) % 60;
    }
    lastSecondChange = nowMs;
    transitionProgress = 0.0;
  }

  // Check for minute change to update colors
  if (currentMinutes != lastMinute) {
    updateColors();
    lastMinute = currentMinutes;
  }

  // Check for 5-minute mark to start animation
  if (!isAnimating && currentMinutes % 5 == 0 && currentMinutes != lastAnimationMinute) {
    isAnimating = true;
    animationStartTime = nowMs;
    lastAnimationMinute = currentMinutes;
    Serial.println("Starting ring animation");
  }

  // Run animation or pattern display
  if (isAnimating) {
    runRingAnimation(nowMs);
  } else {
    switch (patternMode) {
      case 0:
        displayPattern3(nowMs);
        break;
      case 1:
        displayPattern0(nowMs);
        break;
      case 2:
        displayPattern2(nowMs);
        break;
      case 3:
        displayPattern1(nowMs);
        break;
    }
  }

  // Periodically update time from NTP
  if (nowMs - lastNtpUpdateTime >= updateIntervalMs) {
    getTimeFromNTP();
  }
}