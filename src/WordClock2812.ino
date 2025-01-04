#include "config.h"

// This #include statement was automatically added by the Particle IDE.
#include <FastLED.h>

// This #include statement was automatically added by the Particle IDE.
#include <HttpClient.h>

// This #include statement was automatically added by the Particle IDE.
#include <widgets/widget.h>
#include <widgets/wordclock.h>
#include <widgets/date.h>
#include <widgets/weather.h>
#include <widgets/message.h>
#include <widgets/lastfm.h>
#include <widgets/icon.h>
#include <widgets/miflora.h>
#include <widgets/settings_general.h>
#include <widgets/tpm2.h>

// Fonts
#include <fonts/fonts.h>

// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>

// retained SerialLogHandler logHandler;
// #undef NRF_LOG_INFO
// #define NRF_LOG_INFO Serial.printf

FASTLED_USING_NAMESPACE

String particleDeviceName;

#define CONNECTION_TIMEOUT 300
#define LISTENING_TIMEOUT 60

typedef enum {
    OFFLINE = 0,
    LISTENING = 1,
    CONNECTING = 2,
    READY = 3,
    CONNECTED = 4
} ConnectionState;

ConnectionState state = OFFLINE;
system_tick_t lastStateChange = 0;

const char * const iconOffline = "4503";
const char * const iconListening = "17283";
const char * const iconConnecting = "4503";
const char * const iconConnected = "17947";

void startup() {
    System.enableFeature(FEATURE_RETAINED_MEMORY);
    Serial.begin(115200);
}

// STARTUP(startup());
SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

retained WordClockWidget    wordClockWidget;
retained DateWidget         dateWidget(DateWidgetConfig(false, true));
retained WeatherWidget      weatherWidget(WeatherWidgetConfig{WEATHER_APP_ID, true});
retained MessageWidget      messageWidget;
// LastFMWidget                lastFMWidget(LastFMConfig{LASTFM_USER, LASTFM_API_KEY});
retained TPM2Widget         tpm2Widget;

retained SettingsGeneral settingsGeneral(SettingsGeneralConfig{
    font: 1,
    nightShift: true,
    dim: true,
    useEuroDSTRule: true,
    hoursOffset: TIMEZONE,
    textColor: {
        { r: 255, g: 255, b: 255 },
        { r: 255, g: 255, b: 255 },
    },
    brightnessMin: MIN_BRIGHTNESS,
    brightnessMax: MAX_BRIGHTNESS,
    luminanceMin: MIN_LUMINANCE,
    luminanceMax: MAX_LUMINANCE,
    gamma: GAMMA,
    latitude: LATITUDE,
    longitude: LONGITUDE,
    scrollDelay: SCROLL_DELAY,
});

Widget* const widgets[] = {
    &wordClockWidget,
    &dateWidget,
    &weatherWidget,
    // &lastFMWidget,
    &messageWidget,
    &tpm2Widget,
    &settingsGeneral
};

int8_t activeWidget = -1;
int8_t nextWidget, nextLoopWidget = 0;
const int loopWidgets = 3;

#include <utils.h>

// Open a serial terminal and see the device name printed out
void handlerNotification(const char *event, const char *data) {
    messageWidget.processNotificationJSON(data, strlen(data));
}

int handlerNotificationFunction(String data) {
    messageWidget.processNotificationJSON(data.c_str(), data.length());
    return 0;
}

void callbackMessageWidget(char* topic, byte* payload, unsigned int length) {
    messageWidget.processNotificationJSON((char *)payload, length);
}

void getWidgetList(String &list) {
    // 1. Get total lenght of list
    size_t bufSize = 1;
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i) {
        const char *def = widgets[i]->configDef(false);
        size_t len = strlen(def);
        if (len > 0)
            bufSize += 1 + len;
    }

    // 2. Reserve space and fill list
    list.reserve(bufSize);
    list += '[';
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i) {
        const char *def = widgets[i]->configDef(false);
        size_t len = strlen(def);
        if (len > 0) {
            if (list.length() > 1)
                list += ",";
            list += def;
        }
    }
    list += ']';
}

void handlerWidgetList(const char *event, const char *) {
    String list;
    getWidgetList(list);
    Particle.publish(PREFIX_WIDGET_LIST_RESPONSE, list, PRIVATE);
}

String funcWidgetList() {
    String list;
    getWidgetList(list);
    return list;
}

void handlerWidgetDef(const char *event, const char *) {
    size_t prefixLen = strlen(PREFIX_WIDGET_DEF);
    const char *widget = event + prefixLen;
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i)
        if (strcmp(widgets[i]->name(), widget) == 0) {
            char responseEvent[48] = PREFIX_WIDGET_DEF_RESPONSE;
            strcpy(responseEvent + strlen(PREFIX_WIDGET_DEF_RESPONSE), widget);
            Particle.publish(responseEvent, widgets[i]->configDef(true), PRIVATE);
            break;
        }
}

void handlerWidgetGet(const char *event, const char *) {
    size_t prefixLen = strlen(PREFIX_WIDGET_GET);
    const char *widget = event + prefixLen;
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i)
        if (strcmp(widgets[i]->name(), widget) == 0) {
            char responseEvent[48] = PREFIX_WIDGET_GET_RESPONSE;
            strcpy(responseEvent + strlen(PREFIX_WIDGET_GET_RESPONSE), widget);
            Particle.publish(responseEvent, widgets[i]->configGet(), PRIVATE);
            break;
        }
}

void handlerWidgetSet(const char *event, const char *data) {
    size_t prefixLen = strlen(PREFIX_WIDGET_SET);
    const char *widget = event + prefixLen;
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i)
        if (strcmp(widgets[i]->name(), widget) == 0) {
            char responseEvent[48] = PREFIX_WIDGET_GET_RESPONSE;
            strcpy(responseEvent + strlen(PREFIX_WIDGET_GET_RESPONSE), widget);
            widgets[i]->configSet(data);
            Particle.publish(responseEvent, widgets[i]->configGet(), PRIVATE);
            break;
        }
}

void handlerWidgetHookReponse(const char *event, const char *data) {
    size_t prefixLen = strlen(PREFIX_WIDGET_HOOK_RESPONSE);
    const char *widget = event + prefixLen;
    for (size_t i = 0; i < sizeof(widgets) / sizeof(Widget*); ++i) {
        size_t nameLen = strlen(widgets[i]->name());
        if (strncmp(widgets[i]->name(), widget, nameLen) == 0) {
            const char *hookName = widget + nameLen;
            if (*hookName)
                ++hookName;
            widgets[i]->hookHandler(hookName, data);
            break;
        }
    }
}

void handlerWidget(const char *event, const char *data) {
    // remove device specific prefix
    size_t prefixLen = System.deviceID().length() + 1;
    if (strlen(event) < prefixLen)
        return;
    event += prefixLen;

    if (strncmp(event, PREFIX_WIDGET_DEF, strlen(PREFIX_WIDGET_DEF)) == 0)
        handlerWidgetDef(event, data);
    else if (strncmp(event, PREFIX_WIDGET_SET, strlen(PREFIX_WIDGET_SET)) == 0)
        handlerWidgetSet(event, data);
    else if (strcmp(event, PREFIX_WIDGET_LIST) == 0)
        handlerWidgetList(event, data);
    else if (strncmp(event, PREFIX_WIDGET_GET, strlen(PREFIX_WIDGET_GET)) == 0)
        handlerWidgetGet(event, data);
    else if (strncmp(event, PREFIX_WIDGET_HOOK_RESPONSE, strlen(PREFIX_WIDGET_HOOK_RESPONSE)) == 0)
        handlerWidgetHookReponse(event, data);
#if HAL_PLATFORM_BLE
    else if (strcmp(event, PREFIX_WIDGET_BTSCAN) == 0) {
        handlerBluetoothScan(event, data);
    }
#endif
}

// MQTT clients (connecting to Losant and Home Assistant brokers)
Mutex systemLock = Mutex();

void setupSensors();

void setup() {
    startup();
    FastLED.addLeds<PIXEL_TYPE, PIXEL_PIN>(gfx.leds, gfx.size());//.setCorrection(TypicalSMD5050);
    FastLED.clear();
    FastLED.show();

    gfx.setFont(getFont("m3x6").font);
    delay(5000);

    Particle.subscribe("particle/device/name", handlerDeviceName, MY_DEVICES);
    Particle.subscribe("potty/notification", handlerNotification, MY_DEVICES);
    Particle.function("notify", handlerNotificationFunction);

    Particle.subscribe(System.deviceID() + "/" PREFIX_WIDGET, handlerWidget, MY_DEVICES);
    Particle.variable("widgetList", &funcWidgetList);

    setupSensors();

    Particle.connect();
}

void onConnect() {
    LOG(INFO, "Connected");

    theme.setColor(LED_SIGNAL_CLOUD_CONNECTED, 0x00000000); // Set LED_SIGNAL_NETWORK_ON to no color
    theme.apply(); // Apply theme settings

    for (unsigned i = 0; i < (sizeof(widgets) / sizeof(Widget*)); ++i)
        widgets[i]->onConnect();

    Particle.publish("particle/device/name", PRIVATE);
}

void loop() {
    ConnectionState newState = OFFLINE;
    if (WiFi.listening()) {
        newState = LISTENING;
    } else if (WiFi.connecting()) {
        newState = CONNECTING;
    } else if (Particle.connected()) {
        newState = CONNECTED;
    } else if (WiFi.ready()) {
        newState = READY;
    }

    if (state != newState) {
        state = newState;
        lastStateChange = millis();
        switch (state) {
            case OFFLINE:
                iconText.set(iconOffline, "");
                break;
            case LISTENING:
                iconText.set(iconListening, "");
                break;
            case CONNECTING:
                iconText.set(iconConnecting, "");
                break;
            case READY:
                iconText.set(iconConnected, "");
                break;
            case CONNECTED:
                onConnect();
                break;
        }
        iconText.align(gfx);
    }

    switch (state) {
        case OFFLINE:
            if (millis() - lastStateChange > CONNECTION_TIMEOUT * 1000 && !inSleepMode()) {
                lastStateChange = millis();
                WiFi.off();
                WiFi.on();
                WiFi.connect();
            }
            break;
        // case CONNECTING:
        //     if (millis() - lastStateChange > CONNECTION_TIMEOUT * 1000)
        //         WiFi.listen();
        //     break;
        // case LISTENING:
        //     if (millis() - lastStateChange > LISTENING_TIMEOUT * 1000)
        //         WiFi.listen(false);
        //     break;
    }

    EVERY_N_MILLISECONDS(15) {
        if (renderLoop(activeWidget >= 0 ? widgets[activeWidget] : NULL))
            fadeLoop();
    }

    EVERY_N_MILLISECONDS(20) {
        loopHASS();
        if (brightness > 0) {
            if (Particle.connected()) {

                // Is there a widget that wants to be shown?
                UrgencyLevel highestUrgency = normal;
                for (unsigned i = 0; i < (sizeof(widgets) / sizeof(Widget*)); ++i) {
                    UrgencyLevel ul = widgets[i]->urgency();
                    if (highestUrgency < ul) { 
                        highestUrgency = ul;
                        nextWidget = i;
                    }
                }

                // If not, cycle through regular widgets if necessary
                if (activeWidget == nextWidget) {
                    if (highestUrgency == normal && iconText.needTransition()) {
                        if (inSleepMode())
                            nextWidget = 0;
                        else {
                            do {
                                nextWidget = nextLoopWidget;
                                nextLoopWidget = (nextLoopWidget + 1) % loopWidgets;
                            } while (widgets[nextWidget]->urgency() == silent);
                        }
                    } else if (highestUrgency == high) {
                        iconText.resetTimer();
                    }
                }

                if ((highestUrgency == highWithTransition || activeWidget != nextWidget) && iconText.readyForTransition()) {
                    if (activeWidget >= 0)
                        widgets[activeWidget]->afterShow();
                    activeWidget = nextWidget;
                    iconText.startTransition(scrollDown);
                    widgets[activeWidget]->beforeShow();
                }
                if (activeWidget >= 0)
                    widgets[activeWidget]->loop(iconText);
            } else {
                iconText.invalidate();
                EVERY_N_MILLISECONDS(10000) {
                    Particle.connect();
                }
                activeWidget = -1;
            }
        }
    }
}