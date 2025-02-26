#ifndef __CONFIG_H_
#define __CONFIG_H_

// #define DEBUG_ON_LAMATRIX

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN D6
#define PIXEL_TYPE NEOPIXEL

#define FADE_STEP 3

#define AUTO_BRIGHTNESS

/* I'm in the midwest, so this is what I use for my home */
#define TIMEZONE        1

#define HELENE

#ifdef HELENE
// lamatrix, user davomat4, mail dave.weese+wordclock_helene@gmail.com
#define WEATHER_APP_ID      "e073d5376cd65a077b84899a0aa0bbb0"
#else
// lamatrix, user davomat3, mail dave.weese+wordclock@gmail.com
#define WEATHER_APP_ID      "e073d5376cd65a077b84899a0aa0bbb0"
#define LASTFM_USER         "Davomat"
#define LASTFM_API_KEY      "35cadb8679c297db3791b7271ed185c7"
#endif
#define LATITUDE            52.47257
#define LONGITUDE           13.29287

#ifdef DEBUG_ON_LAMATRIX
#define FASTLED_PARTICLE_CLOCKLESS_SPI SPI
#else
#define FASTLED_PARTICLE_CLOCKLESS_SPI SPI1
#endif

#define SCROLL_DELAY        200
#define MIN_SCROLL_CYCLES   2
#define ROTATE_DISPLAY      false
#define LUM_THRESH          2
#define GAMMA               2.5

#ifndef HELENE
#define LOSANT_BROKER "broker.losant.com"
#define LOSANT_DEVICE_ID "5d6c4a0a6cdb3e0006f11f60"
#define LOSANT_ACCESS_KEY "f21356a8-9a49-4a5f-89f5-a7467bb6857b"
#define LOSANT_ACCESS_SECRET "4b72d91cb8e7c855b15106683173a61ef94db9be6c0d25c9bf2206c4d00bc4be"
#endif

// Topic used to subscribe to Losant commands.
#define MQTT_TOPIC_COMMAND "losant/" LOSANT_DEVICE_ID "/command"
// Topic used to publish state to Losant.
#define MQTT_TOPIC_STATE "losant/" LOSANT_DEVICE_ID "/state"

#define HASS
#define HASS_BROKER "192.168.1.1"
#define HASS_ACCESS_USER "mqtt_user"
#define HASS_ACCESS_PASS "mqtt_pass"

#ifdef HELENE
    #define HASS_TOPIC_NAME "WordClock Helene"
    #define LUMINANCE_SENSOR_ANALOG
    #define LUMINANCE_SENSOR_PIN A0
    #define MIN_LUMINANCE       100
    #define MAX_LUMINANCE       4000
    #define MIN_BRIGHTNESS      4
    #define MAX_BRIGHTNESS      255
#else
    #define HASS_TOPIC_NAME "WordClock"
    #define LUMINANCE_SENSOR_BME280_TSL2591
    #define MIN_LUMINANCE       210
    #define MAX_LUMINANCE       455
    #define MIN_BRIGHTNESS      10
    #define MAX_BRIGHTNESS      255
#endif

#define HASS_TOPIC_PREFIX "homeassistant/light/"
#define HASS_TOPIC_STATE_SUFFIX "/status"
#define HASS_TOPIC_SET_SUFFIX "/set"
#define HASS_TOPIC_CONFIG_SUFFIX "/config"

#endif // __CONFIG_H_
