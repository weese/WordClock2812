#include <widgets/icon_text.h>

#undef retained
#define retained

// Log message to cloud, message is a printf-formatted string
void debug(String message, int value) {
    char msg [50];
    sprintf(msg, message.c_str(), value);
    Particle.publish("DEBUG", msg, PRIVATE);
}

void debug(String message) {
    debug(message, 0);
}

retained HttpClient http;
retained FastLED_Matrix<11, 10, RowWise<4> > gfx;
IconText iconText(http, MIN_SCROLL_CYCLES, SCROLL_DELAY, ROTATE_DISPLAY);

fract8 fadeFract = 255;
fract8 brightness = 255;
int targetBrightness = 255;
int luminance = 0;
int deltaBrightness = 0;

// SunSet sun;
LEDSystemTheme theme; // Enable custom theme

uint16_t lastTimestamp = UINT16_MAX;

// Home Assistent state
CRGB hassRGB = CRGB::White;
uint16_t hassColorTemp = 325; // pure white
fract8 hassBrightness = 0;
bool hassOn = false;

// reset the system after 10 seconds if the application is unresponsive
// ApplicationWatchdog wd(10000, System.reset);

uint32_t lumSum = 0;
uint8_t lumCount = 0;

uint8_t lum2brightness(int luminance);

void updateBrightness() {
#ifdef AUTO_BRIGHTNESS
    targetBrightness = lum2brightness(luminance);
#else
    targetBrightness = BRIGHTNESS_INITIAL;
#endif
}

void readLuminance() {
#ifdef LUMINANCE_SENSOR_PIN
    // compute new brightness
    lumSum += analogRead(LUMINANCE_SENSOR_PIN);
    if (++lumCount == 4) {
        int newLuminance = lumSum / 4;
        int delta = newLuminance - luminance;
        if (delta < -LUM_THRESH || delta > LUM_THRESH || newLuminance <= MIN_LUMINANCE || newLuminance >= MAX_LUMINANCE)
            luminance = newLuminance;
        lumSum = 0;
        lumCount = 0;
    }
    updateBrightness();
#endif
}

bool renderLoop(Widget* widget) {

#ifdef POTTY
    if (gfx.leds[255] == CRGB(0) && gfx.leds[254] == CRGB(0) && gfx.leds[241] == CRGB(0) && gfx.leds[240] == CRGB(0))
#endif
        readLuminance();

    // take a snapshot of the current state as source for fading
    // nblend(src, dst, NUM_LEDS, fadeFract);
    // fadeFract = 0;
    fadeFract = 255;
    if (widget)
        return widget->render(gfx, iconText);
    else
        return iconText.render(gfx);
}

void updateLeds() {
    // blend(src, dst, leds, NUM_LEDS, fadeFract);
    FastLED.show(hassBrightness > 1 ? hassBrightness : brightness);
}

void fadeLoop() {
    // bool updateRequired = (fadeFract != 255);
    bool updateRequired = true;

    if (brightness != targetBrightness) {
        updateRequired = true;
        if (brightness < targetBrightness) {
            if (targetBrightness - brightness < FADE_STEP) {
                brightness = targetBrightness;
            } else {
                brightness += FADE_STEP;
            }
        } else {
            if (brightness - targetBrightness < FADE_STEP) {
                brightness = targetBrightness;
            } else {
                brightness -= FADE_STEP;
            }
        }
    }

    if (updateRequired) {
        updateLeds();
    }
}

void callbackHass(char* topic, byte* payload, unsigned int length);

// MQTT clients (connecting to Losant and Home Assistant brokers)
retained MQTT clientHass(HASS_BROKER, 1883, callbackHass, 500);

void sendDiscoveryToken() {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    String topic = HASS_TOPIC_PREFIX;
    topic += particleDeviceName;
    char buffer[400] = "";
    root["~"] = topic.c_str();
    root["name"] = "Potty";
    root["unique_id"] = particleDeviceName.c_str();
    root["cmd_t"] = "~" HASS_TOPIC_SET_SUFFIX;
    root["stat_t"] = "~" HASS_TOPIC_STATE_SUFFIX;
    root["schema"] = "json";
    root["rgb"] = true;
    root["brightness"] = false;
    // root["transition"]=2;
    root["effect"] = false;
    // JsonArray& list = root.createNestedArray("effect_list");
    // for (unsigned i = 0; i < sizeof(langList) / sizeof(langList[0]); ++i) {
    //     list.add(langList[i]);
    // }
    root.printTo(buffer, sizeof(buffer));

    topic += HASS_TOPIC_CONFIG_SUFFIX;
    clientHass.publish(topic, buffer, true);
}

void sendStateHass() {
    StaticJsonBuffer<300> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& color = root.createNestedObject("color");

    color["r"] = hassRGB.r;
    color["g"] = hassRGB.g;
    color["b"] = hassRGB.b;

    root["color_temp"] = hassColorTemp;
    root["state"] = (hassOn) ? "ON" : "OFF";
    // root["effect"] = langList[(lang.dialect << 1) + lang.fullSentence];

    char buffer[300];
    root.printTo(buffer, sizeof(buffer));

    String topic = HASS_TOPIC_PREFIX;
    topic += particleDeviceName;
    topic += HASS_TOPIC_STATE_SUFFIX;
    clientHass.publish(topic, buffer, true);
}

// From http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/

    // Start with a temperature, in Kelvin, somewhere between 1000 and 40000.  (Other values may work,
    //  but I can't make any promises about the quality of the algorithm's estimates above 40000 K.)
    
CRGB colorTemperatureToRGB(double kelvin) {
    double temp = kelvin / 100.0;
    uint8_t red, green, blue;
    if (temp <= 66) { 
        red = 255; 
        green = constrain(99.4708025861 * log(temp) - 161.1195681661, 0.0, 255.0);
        if (temp <= 19) {
            blue = 0;
        } else {
            blue = constrain(138.5177312231 * log(temp - 10) - 305.0447927307, 0.0, 255.0);
        }
    } else {
        red = constrain(329.698727446 * pow(temp - 60, -0.1332047592), 0.0, 255.0);
        green = constrain(288.1221695283 * pow(temp - 60, -0.0755148492), 0.0, 255.0);
        blue = 255;
    }
    return CRGB(red, green, blue);
}


// Callback signature for MQTT subscriptions
void callbackHass(char* topic, byte* payload, unsigned int length) {
    /*Parse the command payload.*/
    StaticJsonBuffer<300> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject((char*)payload);
    if (root.containsKey("state")) {
        hassOn = (strcmp(root["state"], "ON") == 0);
    }
    if (root.containsKey("brightness")) {
        hassBrightness = root["brightness"];
    }
    if (root.containsKey("color")) {
        JsonObject& rgb = root["color"];
        hassRGB.setRGB(rgb["r"], rgb["g"], rgb["b"]);
    } else if (root.containsKey("color_temp")) {
        hassColorTemp = root["color_temp"];
        hassRGB = colorTemperatureToRGB(3000000.0 / hassColorTemp - 2600);
    }
    // if (root.containsKey("effect")) {
    //     for (unsigned i = 0; i < sizeof(langList) / sizeof(langList[0]); ++i) {
    //         if (strcmp(root["effect"], langList[i]) == 0) {
    //             lang.dialect = i >> 1;
    //             lang.fullSentence = (i & 1) == 1;
    //         }
    //     }
    // }
    lastTimestamp = UINT16_MAX;
    // sendStateHass();
}

// void callbackPotty(const char* topic, byte* payload, unsigned int length) {
//     /*Parse the command payload.*/
//     StaticJsonBuffer<300> jsonBuffer;
//     JsonObject& root = jsonBuffer.parseObject((char*)payload);
//     iconText.set(root["icon"].as<String>(), root["text"].as<String>(), 0);
//     iconText.init(gfx);
// }


bool connectHassOnDemand() {
    return false; 
    if (particleDeviceName.length()) {
        if (clientHass.isConnected())
            return true;
    
        clientHass.connect(
            particleDeviceName.c_str(),
            HASS_ACCESS_USER,
            HASS_ACCESS_PASS);
        
        bool bConn = clientHass.isConnected();
        if (bConn) {
            debug("Connected to HASS");
            String topic = HASS_TOPIC_PREFIX;
            topic += particleDeviceName;
            topic += HASS_TOPIC_SET_SUFFIX;
            clientHass.subscribe(topic);
            sendDiscoveryToken();
        }
        return bConn;
    }
    return false;
}

void loopHASS() {
    if (!connectHassOnDemand()) {
        return;
    }
    // Loop the MQTT client
    clientHass.loop();
    EVERY_N_HOURS(24) {
        sendDiscoveryToken();
    }
}


uint8_t sensorIdx = 0;

// Open a serial terminal and see the device name printed out
void handlerDeviceName(const char *topic, const char *data) {
    Serial.println(String("Received device topic ") + topic + "="+data);
    if (strcmp(topic, "particle/device/name") == 0) {
        particleDeviceName = data;
        Serial.println(String("Received device name ") + data);
    }
}

int handlerSetBrightnessDeltaFunction(String data) {
    deltaBrightness = data.toInt();
    updateBrightness();
    return 0;
}

void initBluetooth() {
#if HAL_PLATFORM_BLE
    BleScanParams scanParams;
    scanParams.version = BLE_API_VERSION;
    scanParams.size = sizeof(BleScanParams);
    BLE.getScanParameters(scanParams);              // Get the default scan parameters
    scanParams.timeout = 3000;                       // Change timeout to 30 seconds
    // Scanning for both 1 MBPS and CODED PHY simultaneously requires scanning window <= 1/2 the scanning interval.
    // We will widen the window to 2/3 of the interval so automatic override will be tested in simultaneous mode
    scanParams.window = (2 * scanParams.interval) / 3 ;
    // First, scan and log with standard PHYS_1MBPS
    scanParams.scan_phys = BLE_PHYS_1MBPS;
    BLE.setScanParameters(scanParams);
#endif
}
