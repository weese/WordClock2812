#include "config.h"

// This #include statement was automatically added by the Particle IDE.
#include <FastLED.h>

// This #include statement was automatically added by the Particle IDE.
#include <HttpClient.h>

// This #include statement was automatically added by the Particle IDE.
#include <widgets/fastled_matrix_wc.h>
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
#include <SparkJson.h>

// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>

// This #include statement was automatically added by the Particle IDE.
#include <Wire.h>

// This #include statement was automatically added by the Particle IDE.
#include <SunSet.h>

// This #include statement was automatically added by the Particle IDE.
#include <SparkTime.h>

#include <BlueDot_BME280_TSL2591.h>

FASTLED_USING_NAMESPACE

String particleDeviceName;

#include <utils.h>

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

const char * const textOffline = "";
const char * const textListening = "";
const char * const textConnecting = "";
const char * const textConnected = "";

void startup() {
    System.enableFeature(FEATURE_RETAINED_MEMORY);
    Serial.begin(115200);
}

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

retained WordClockWidget wordClockWidget;
retained DateWidget dateWidget(wordClockWidget.rtc, DateWidgetConfig(false, true));

retained WeatherWidget weatherWidget(WeatherWidgetConfig{WEATHER_APP_ID, LATITUDE, LONGITUDE});
retained MessageWidget messageWidget;
// LastFMWidget lastFMWidget(LastFMConfig{LASTFM_USER, LASTFM_API_KEY});
retained TPM2Widget tpm2Widget;

SettingsGeneralConfig SettingsGeneral::config = {
    font: 3,
    dim: true,
    colorText: { r: 255, g: 255, b: 255 },
    brightnessMin: MIN_BRIGHTNESS,
    brightnessMax: MAX_BRIGHTNESS,
    luminanceMin: MIN_LUMINANCE,
    luminanceMax: MAX_LUMINANCE,
    gamma: GAMMA,
};
retained SettingsGeneral settingsGeneral;

Widget* const widgets[] = {
    &wordClockWidget,
    &dateWidget,
    // &weatherWidget,
    // &lastFMWidget,
    &messageWidget,
    &tpm2Widget,
    &settingsGeneral
};

int8_t activeWidget = -1;
int8_t nextWidget, nextLoopWidget = 0;
const int loopWidgets = 2;

// Open a serial terminal and see the device name printed out
void handlerNotification(const char *event, const char *data) {
    messageWidget.processNotificationJSON(data);
}

int handlerNotificationFunction(String data) {
    messageWidget.processNotificationJSON(data.c_str());
    return 0;
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

UDP UDPClient;
SparkTime rtc;
SunSet sun;
BlueDot_BME280_TSL2591 bme280;
BlueDot_BME280_TSL2591 tsl2591;

double illuminance = 0;
double temperature = 0;
double pressure = 0;
double humidity = 0;

Thread* measureWorker;
Mutex systemLock = Mutex();

// namespace NSFastLED {

// uint16_t XY(uint8_t x, uint8_t y) {
//     if (y & 1) {
//         x = 10 - x;
//     }
//     return 4 + y * 11 + x;
// }

// }


struct CRGB computeColorOfTheDay(unsigned long currentTime) {
	sun.setCurrentDate(rtc.year(currentTime), rtc.month(currentTime), rtc.day(currentTime));
	
	/* Check to see if we need to update our timezone value */
	if (rtc.isEuroDST(currentTime))
		sun.setTZOffset(TIMEZONE + 1);
	else
		sun.setTZOffset(TIMEZONE);
	
    uint16_t sunrise = sun.calcSunrise();
    uint16_t sunset = sun.calcSunset();
    uint16_t minOfTheDay = rtc.hour(currentTime) * 60 + rtc.minute(currentTime);

    if (minOfTheDay <= sunrise - 50) {
        return Candle;
    } else if (minOfTheDay <= sunrise) {
        return blend(CRGB(DirectSunlight), CRGB(Candle), (sunrise - minOfTheDay) * 5);
    } else if (minOfTheDay <= sunset - 30) {
        return CRGB(DirectSunlight);
    } else if (minOfTheDay <= sunset) {
        return blend(CRGB(Candle), CRGB(DirectSunlight), (sunset - minOfTheDay) * 8);
    } else {
        return Candle;
    }
}

uint8_t getBrightness() {
    illuminance = tsl2591.readIlluminance_TSL2591();
    double result = log(illuminance + 1) * 50 - 200;
    if (result > 255) {
        return 255;
    } else if (result < MIN_BRIGHTNESS) {
        return MIN_BRIGHTNESS;
    } else {
        return result;
    }
}

void callbackHass(char* topic, byte* payload, unsigned int length);

// MQTT clients (connecting to Losant and Home Assistant brokers)
MQTT client(LOSANT_BROKER, 1883, NULL);

uint8_t clamp(double x) {
    if (x < 0) { return 0; }
    if (x > 255) { return 255; }
    return x;
}

bool connectOnDemand() {
    if (client.isConnected())
        return true;
    
    systemLock.lock();
    client.connect(
        LOSANT_DEVICE_ID,
        LOSANT_ACCESS_KEY,
        LOSANT_ACCESS_SECRET);
    
    bool bConn = client.isConnected();
    if (bConn) {
        debug("Connected to Losant");
        client.subscribe(MQTT_TOPIC_COMMAND);
    }
    systemLock.unlock();
    return bConn;
}

void loopLosant() {
    if (!connectOnDemand()) {
        return;
    }

    // Build the json payload:
    // { “data” : { “temperature” : val, "humidity": val, “pressure” : val, “illuminance” : val }}
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& state = jsonBuffer.createObject();
    state["temperature"] = temperature;
    state["humidity"] = humidity;
    state["pressure"] = pressure;
    state["illuminance"] = illuminance;
    root["data"] = state;

    // Get JSON string
    char buffer[300];
    root.printTo(buffer, sizeof(buffer));

    // Loop the MQTT client
    systemLock.lock();
    client.loop();
    client.publish(MQTT_TOPIC_STATE, buffer);
    systemLock.unlock();
}


os_thread_return_t measureLoop(void* param) {
    while (true) {
        targetBrightness = getBrightness();
        EVERY_N_MILLISECONDS(1000) {
            // Don't read sensors while we are fading ... the blocking reads would cause glitches
            switch (sensorIdx) {
                case 0:
                    temperature = bme280.readTempC();
                    break;
                case 1:
                    pressure = bme280.readPressure();
                    break;
                case 2:
                    humidity = bme280.readHumidity();
                    loopLosant();
            }
            if (++sensorIdx == 3) {
                sensorIdx = 0;
            }
            
            // debug("target %d", targetBrightness);
            // Particle.publish("temperature", String(temperature), 60, PRIVATE);
            // Particle.publish("pressure", String(pressure), 60, PRIVATE);
            // Particle.publish("humidity", String(humidity), 60, PRIVATE);
            // Particle.publish("illuminance", String(illuminance), 60, PRIVATE);
        }
        os_thread_yield();
    }
}

void setup() {
    startup();
    LOG(INFO, "Started");
    FastLED.addLeds<PIXEL_TYPE, PIXEL_PIN>(gfx.leds, gfx.size());//.setCorrection(TypicalSMD5050);
    FastLED.clear();
    FastLED.show();

    gfx.setFont(getFont("m3x6").font);
    delay(5000);

    Particle.subscribe("particle/device/name", handlerDeviceName);
    Particle.publish("particle/device/name");
    Particle.subscribe("potty/notification", handlerNotification, MY_DEVICES);
    Particle.function("notify", handlerNotificationFunction);

    Particle.subscribe(System.deviceID() + "/" PREFIX_WIDGET, handlerWidget, MY_DEVICES);
    Particle.variable("widgetList", &funcWidgetList);

    theme.setColor(LED_SIGNAL_CLOUD_CONNECTED, 0x00000000); // Set LED_SIGNAL_NETWORK_ON to no color
    theme.apply(); // Apply theme settings
    Wire.setSpeed(400000);


    // for (uint8_t i=0;i<10;++i) {
    //     gfx.drawFastVLine(i,i,2,0b0001000001000010 * (i+2));
    //     // CRGB col[2];
    //     // col[0] = CRGB(i*2, i*6, i*4);
    //     // col[1] = CRGB(i*4, i*6, i*4);
    //     // gfx.writeHPixels(i,i,col, 2);
    // }
    // gfx.mirror();
    // gfx.flip();
    FastLED.show();
    
    connectHassOnDemand();
    
    Wire.begin();
    bme280.parameter.I2CAddress = 0x77;                 //The BME280 is hardwired to use the I2C Address 0x77
    tsl2591.parameter.I2CAddress = 0x29;

    //*********************************************************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************        
    
    //Here we can configure the TSL2591 Light Sensor
    //First we set the gain value
    //Higher gain values are better for dimmer light conditions, but lead to sensor saturation with bright light 
    //We can choose among four gain values:
    
    //0b00:    Low gain mode
    //0b01:    Medium gain mode
    //0b10:    High gain mode
    //0b11:    Maximum gain mode
    
    tsl2591.parameter.gain = 0b10;
    
    //Longer integration times also helps in very low light situations, but the measurements are slower
    
    //0b000:   100ms (max count = 37888)
    //0b001:   200ms (max count = 65535)
    //0b010:   300ms (max count = 65535)
    //0b011:   400ms (max count = 65535)
    //0b100:   500ms (max count = 65535)
    //0b101:   600ms (max count = 65535)
    
    tsl2591.parameter.integration = 0b000;    
    
    //The values for the gain and integration times are written transfered to the sensor through the function config_TSL2591
    //This function powers the device ON, then configures the sensor and finally powers the device OFF again 
       
    tsl2591.config_TSL2591();
    
    

    //*********************************************************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //Now choose on which mode your device will run
    //On doubt, just leave on normal mode, that's the default value
    
    //0b00:     In sleep mode no measurements are performed, but power consumption is at a minimum
    //0b01:     In forced mode a single measured is performed and the device returns automatically to sleep mode
    //0b11:     In normal mode the sensor measures continually (default value)
    
    bme280.parameter.sensorMode = 0b11;                   //Choose sensor mode
    
    
    
    //*********************** TSL2591 *************************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //Great! Now set up the internal IIR Filter
    //The IIR (Infinite Impulse Response) filter suppresses high frequency fluctuations
    //In short, a high factor value means less noise, but measurements are also less responsive
    //You can play with these values and check the results!
    //In doubt just leave on default
    
    //0b000:      factor 0 (filter off)
    //0b001:      factor 2
    //0b010:      factor 4
    //0b011:      factor 8
    //0b100:      factor 16 (default value)
    
    bme280.parameter.IIRfilter = 0b100;                    //Setup for IIR Filter
    
    
    
    //************************** BME280 ***********************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //Next you'll define the oversampling factor for the humidity measurements
    //Again, higher values mean less noise, but slower responses
    //If you don't want to measure humidity, set the oversampling to zero
    
    //0b000:      factor 0 (Disable humidity measurement)
    //0b001:      factor 1
    //0b010:      factor 2
    //0b011:      factor 4
    //0b100:      factor 8
    //0b101:      factor 16 (default value)
    
    bme280.parameter.humidOversampling = 0b101;            //Setup Humidity Oversampling
    
    
    
    //************************** BME280 ***********************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //Now define the oversampling factor for the temperature measurements
    //You know now, higher values lead to less noise but slower measurements
    
    //0b000:      factor 0 (Disable temperature measurement)
    //0b001:      factor 1
    //0b010:      factor 2
    //0b011:      factor 4
    //0b100:      factor 8
    //0b101:      factor 16 (default value)
    
    bme280.parameter.tempOversampling = 0b101;             //Setup Temperature Ovesampling
    
    
    
    //************************** BME280 ***********************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //Finally, define the oversampling factor for the pressure measurements
    //For altitude measurements a higher factor provides more stable values
    //On doubt, just leave it on default
    
    //0b000:      factor 0 (Disable pressure measurement)
    //0b001:      factor 1
    //0b010:      factor 2
    //0b011:      factor 4
    //0b100:      factor 8
    //0b101:      factor 16 (default value)
    
    bme280.parameter.pressOversampling = 0b101;            //Setup Pressure Oversampling 
    
    
    
    //************************** BME280 ***********************************
    //*************ADVANCED SETUP - SAFE TO IGNORE!************************
    
    //For precise altitude measurements please put in the current pressure corrected for the sea level
    //On doubt, just leave the standard pressure as default (1013.25 hPa)
    
    bme280.parameter.pressureSeaLevel = 1013.25;           //default value of 1013.25 hPa
    
    //Now write here the current average temperature outside (yes, the outside temperature!)
    //You can either use the value in Celsius or in Fahrenheit, but only one of them (comment out the other value)
    //In order to calculate the altitude, this temperature is converted by the library into Kelvin
    //For slightly less precise altitude measurements, just leave the standard temperature as default (15°C)
    //Remember, leave one of the values here commented, and change the other one!
    //If both values are left commented, the default temperature of 15°C will be used
    //But if both values are left uncommented, then the value in Celsius will be used    
    
    bme280.parameter.tempOutsideCelsius = 20;              //default value of 20°C
    //bme280.parameter.tempOutsideFahrenheit = 59;           //default value of 59°F



    //*********************************************************************
    //*************ADVANCED SETUP IS OVER - LET'S CHECK THE CHIP ID!*******
    
    if (bme280.init_BME280() != 0x60) {
        debug("Ops! BME280 could not be found!");
    } else {
        debug("BME280 detected!");
        Particle.variable("temperature", temperature);
        Particle.variable("pressure", pressure);
        Particle.variable("humidity", humidity);
    }
    
    if (tsl2591.init_TSL2591() != 0x50) {
        debug("Ops! TSL2591 could not be found!");
    } else {
        debug("TSL2591 detected!");
        Particle.variable("illuminance", illuminance);
        brightness = targetBrightness = getBrightness();
    }
    
    IPAddress myIP = WiFi.localIP();
    
    measureWorker = new Thread(NULL,  measureLoop);
    debug("Initialized");

    Particle.connect();
}

void onConnect() {
    LOG(INFO, "Connected");
    // Print your device IP Address via serial
    Serial.printf("Application>\tWifi IP: %s\n", WiFi.localIP().toString().c_str());

    theme.setColor(LED_SIGNAL_CLOUD_CONNECTED, 0x00000000); // Set LED_SIGNAL_NETWORK_ON to no color
    theme.apply(); // Apply theme settings

    // connectHassOnDemand();
    // setupSSDP();
    for (unsigned i = 0; i < (sizeof(widgets) / sizeof(Widget*)); ++i) {
        LOG(INFO, "onConnect(%i)", i);
        widgets[i]->onConnect();
    }
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
                iconText.set(iconOffline, textOffline);
                break;
            case LISTENING:
                iconText.set(iconListening, textListening);
                break;
            case CONNECTING:
                iconText.set(iconConnecting, textConnecting);
                break;
            case READY:
                iconText.set(iconConnected, textConnected);
                break;
            case CONNECTED:
                onConnect();
                break;
        }
        iconText.align(gfx);
    }

    switch (state) {
        case OFFLINE:
            if (millis() - lastStateChange > CONNECTION_TIMEOUT * 1000) {
                lastStateChange = millis();
                WiFi.connect();
            }
            break;
        case CONNECTING:
            if (millis() - lastStateChange > CONNECTION_TIMEOUT * 1000)
                WiFi.listen();
            break;
        case LISTENING:
            if (millis() - lastStateChange > LISTENING_TIMEOUT * 1000)
                WiFi.listen(false);
            break;
    }

    EVERY_N_MILLISECONDS(15) {
        // loopHASS();
        // loopSSDP();
        if (renderLoop(activeWidget >= 0 ? widgets[activeWidget] : NULL))
            fadeLoop();
    }

    EVERY_N_MILLISECONDS(20) {
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
                        do {
                            nextWidget = nextLoopWidget;
                            nextLoopWidget = (nextLoopWidget + 1) % loopWidgets;
                        } while (widgets[nextWidget]->urgency() == silent);
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
                EVERY_N_MILLISECONDS(10000) {
                    Particle.connect();
                }
                activeWidget = -1;
            }
        }
    }

    os_thread_yield();
}