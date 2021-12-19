#include <widgets/fastled_matrix.h>
#include <widgets/fastled_matrix_wc.h>
#include <widgets/icon_text.h>
#include <math.h>

#define ANNOUNCE_HASS       1
#define ANNOUNCE_WORDCLOCK  2
#define ANNOUNCE_SETTINGS   4

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

#ifdef DEBUG_ON_LAMATRIX
retained FastLED_Matrix<32, 8, ColumnWise> gfx;
IconText iconText(http, 8, MIN_SCROLL_CYCLES, true);
#else
retained FastLED_Matrix<11, 10, RowWise<4> > gfx;
IconText iconText(http, 10, MIN_SCROLL_CYCLES, ROTATE_DISPLAY);
#endif

fract8 brightness = 10;
int targetBrightness = 10;

LEDSystemTheme theme; // Enable custom theme

// Home Assistent state
uint16_t hassColorTemp = 325; // pure white
uint16_t lastTimestamp = UINT16_MAX;


bool renderLoop(Widget* widget) {
    if (widget)
        return widget->render(gfx, iconText);
    else
        return iconText.render(gfx);
}

void fadeLoop() {
    if (brightness < targetBrightness) {
        if (targetBrightness - brightness < FADE_STEP) {
            brightness = targetBrightness;
        } else {
            brightness += FADE_STEP;
        }
    } else if (brightness > targetBrightness) {
        if (brightness - targetBrightness < FADE_STEP) {
            brightness = targetBrightness;
        } else {
            brightness -= FADE_STEP;
        }
    }
    FastLED.show(brightness);
}

void callbackHass(char* topic, byte* payload, unsigned int length);

// MQTT clients (connecting to Losant and Home Assistant brokers)
retained MQTT clientHass(HASS_BROKER, 1883, 500, callbackHass);

void sendDiscoveryToken() {
    String topic = HASS_TOPIC_PREFIX;
    topic += particleDeviceName;
    topic += HASS_TOPIC_CONFIG_SUFFIX;

    String dialects;
    for (unsigned i = 0; i < sizeof(WordClockWidget::DIALECT_LIST) / sizeof(WordClockWidget::DIALECT_LIST[0]); ++i) {
        if (dialects.length() > 0)
            dialects += "\",\"";
        dialects += WordClockWidget::DIALECT_LIST[i];
    }

    clientHass.publish(topic, String::format(
                "{\"~\":\"" HASS_TOPIC_PREFIX "%s\",\"name\":\"WordClock\",\"unique_id\":\"%s\","
                "\"cmd_t\":\"~" HASS_TOPIC_SET_SUFFIX "\",\"stat_t\":\"~" HASS_TOPIC_STATE_SUFFIX "\","
                "\"schema\":\"json\",\"rgb\":true,\"brightness\":false,\"effect\":true,\"effect_list\":[\"%s\"]}",
                particleDeviceName.c_str(),
                System.deviceID().c_str(),
                dialects.c_str()), true);
}

void announceState(uint8_t mask) {
    String topic;
    
    if (mask & ANNOUNCE_HASS) {
        topic = HASS_TOPIC_PREFIX;
        topic += particleDeviceName;
        topic += HASS_TOPIC_STATE_SUFFIX;

        const SettingsGeneralConfig &config = getGeneralConfig();
        clientHass.publish(topic, String::format(
                    "{\"color\":{\"r\":%i,\"g\":%i,\"b\":%i},\"color_temp\":%i,\"state\":\"%s\",\"effect\":\"%s\"}",
                    config.textColor[0].r,
                    config.textColor[0].g,
                    config.textColor[0].b,
                    hassColorTemp,
                    config.nightShift ? "OFF" : "ON",
                    WordClockWidget::DIALECT_LIST[(wordClockWidget.config.dialect << 1) + wordClockWidget.config.fullSentence]), true);
    }
    
    if (mask & ANNOUNCE_WORDCLOCK) {
        topic = PREFIX_WIDGET_GET_RESPONSE;
        topic += wordClockWidget.name();
        Particle.publish(topic, wordClockWidget.configGet(), PRIVATE);
    }

    if (mask & ANNOUNCE_SETTINGS) {
        topic = PREFIX_WIDGET_GET_RESPONSE;
        topic += settingsGeneral.name();
        Particle.publish(topic, settingsGeneral.configGet(), PRIVATE);
    }
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
    JSONValue outerObj = JSONValue::parseCopy((const char *)payload, length);
    JSONObjectIterator iter(outerObj);

    uint8_t mask = ANNOUNCE_HASS;
    SettingsGeneralConfig &config = getGeneralConfig();
    while (iter.next()) {
        if (iter.name() == "state") {
            resetFading();
            config.nightShift = (iter.value().toString() == "OFF");
            mask |= ANNOUNCE_SETTINGS;
        }

        if (iter.name() == "brightness") {
            config.dim = false;
            config.brightnessMax = iter.value().toInt();
            mask |= ANNOUNCE_SETTINGS;
        }

        if (iter.name() == "color") {
            resetFading();
            JSONObjectIterator iterRGB(iter.value());
            while (iterRGB.next()) {
                if (iterRGB.name() == "r")
                    config.textColor[0].r = iterRGB.value().toInt();
                if (iterRGB.name() == "g")
                    config.textColor[0].g = iterRGB.value().toInt();
                if (iterRGB.name() == "b")
                    config.textColor[0].b = iterRGB.value().toInt();
            }
            config.textColor[1] = config.textColor[0];
            mask |= ANNOUNCE_SETTINGS;
        }
        
        if (iter.name() == "color_temp") {
            resetFading();
            hassColorTemp = iter.value().toInt();
            config.textColor[0] = colorTemperatureToRGB(3000000.0 / hassColorTemp - 2600);
            config.textColor[1] = config.textColor[0];
            mask |= ANNOUNCE_SETTINGS;
        }

        if (iter.name() == "effect") {
            for (unsigned i = 0; i < sizeof(WordClockWidget::DIALECT_LIST) / sizeof(WordClockWidget::DIALECT_LIST[0]); ++i) {
                if (iter.value().toString() == WordClockWidget::DIALECT_LIST[i]) {
                    wordClockWidget.config.dialect = i >> 1;
                    wordClockWidget.config.fullSentence = (i & 1) == 1;
                }
            }
            mask |= ANNOUNCE_WORDCLOCK;
        }
    }

    lastTimestamp = UINT16_MAX;
    announceState(mask);
}

bool connectHassOnDemand() {
    if (clientHass.isConnected())
        return true;

    if (particleDeviceName.length() == 0)
        return false;

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

void loopHASS() {
    if (!connectHassOnDemand())
        return;

    // Loop the MQTT client
    clientHass.loop();
    EVERY_N_HOURS(24) {
        sendDiscoveryToken();
    }
}

void handlerDeviceName(const char *topic, const char *data) {
    particleDeviceName = data;
}
