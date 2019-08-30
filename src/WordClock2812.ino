// This #include statement was automatically added by the Particle IDE.
#include <Wire.h>

// This #include statement was automatically added by the Particle IDE.
#include <SunSet.h>

// This #include statement was automatically added by the Particle IDE.
#include <SparkTime.h>

// This #include statement was automatically added by the Particle IDE.
#include <FastLED.h>

#include <BlueDot_BME280_TSL2591.h>

FASTLED_USING_NAMESPACE

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define NUM_LEDS 114
#define PIXEL_PIN D6
#define PIXEL_TYPE NEOPIXEL

#define FADE_STEP 3

#define LANG_OSSI       0
#define LANG_WESSI      1
#define LANG_RHEIN_RUHR 2

/* I'm in the midwest, so this is what I use for my home */
#define LATITUDE        52.52437
#define LONGITUDE       13.41053
#define TIMEZONE        1


//   0123 4567 89a
// 0 ESKI STLF ÜNF
// 1 ZEHN ZWAN ZIG
// 2 DREI VIER TEL
// 3 TGNA CHVO RJM
// 4 HALB QZWÖ LFP
// 5 ZWEI NSIE BEN
// 6 KDRE IRHF ÜNF
// 7 ELFN EUNV IER
// 8 WACH TZEH NRS
// 9 BSEC HSFM UHR


// row, column, length    rcl
#define WORD_ES         0x002
#define WORD_IST        0x033
#define WORD_FUNF       0x074
#define WORD_ZEHN       0x104
#define WORD_ZWANZIG    0x147
#define WORD_DREI       0x204
#define WORD_VIERTEL    0x247
#define WORD_NACH       0x324
#define WORD_VOR        0x363
#define WORD_HALB       0x404
#define WORD_ZWOLF      0x455
#define WORD_ZWEI       0x504
#define WORD_EIN        0x523
#define WORD_EINS       0x524
#define WORD_SIEBEN     0x556
#define WORD_DREI2      0x614
#define WORD_FUNF2      0x674
#define WORD_ELF        0x703
#define WORD_NEUN       0x734
#define WORD_VIER       0x774
#define WORD_ACHT       0x814
#define WORD_ZEHN2      0x854
#define WORD_SECHS      0x915
#define WORD_UHR        0x983

const uint16_t HOUR_WORDS[12] = {
    WORD_ZWOLF,
    WORD_EINS,
    WORD_ZWEI,
    WORD_DREI2,
    WORD_VIER,
    WORD_FUNF2,
    WORD_SECHS,
    WORD_SIEBEN,
    WORD_ACHT,
    WORD_NEUN,
    WORD_ZEHN2,
    WORD_ELF
};

CRGB leds[NUM_LEDS];
CRGB src[NUM_LEDS];
CRGB dst[NUM_LEDS];
fract8 fadeFract = 255;
fract8 brightness = 255;
fract8 targetBrightness = 255;

struct {
    uint8_t dialect:2;
    uint8_t fullSentence:1;
} lang = { LANG_OSSI, false };

UDP UDPClient;
SparkTime rtc;
SunSet sun;
LEDSystemTheme theme; // Enable custom theme
BlueDot_BME280_TSL2591 bme280;
BlueDot_BME280_TSL2591 tsl2591;

uint16_t lastMinOfTheDay = 0xffff;

CRGB palette[10];

double illuminance = 0;
double temperature = 0;
double pressure = 0;
double humidity = 0;



// Log message to cloud, message is a printf-formatted string
void debug(String message, int value = 0) {
    char msg [50];
    sprintf(msg, message.c_str(), value);
    Spark.publish("DEBUG", msg);
}


// namespace NSFastLED {

// uint16_t XY(uint8_t x, uint8_t y) {
//     if (y & 1) {
//         x = 10 - x;
//     }
//     return 4 + y * 11 + x;
// }

// }

void showWord(uint16_t word) {
    uint8_t row = word >> 8;
    uint8_t col = (word >> 4) & 0xf;
    uint8_t len = word & 0xf;
    if (row & 1) {
        col = 11 - col - len;
    }
    
    uint8_t first = 4 + row * 11 + col;
    uint8_t last = first + len;
    for (uint8_t dot = first; dot != last; ++dot) {
        leds[dot] = palette[row];
    }
}

void showHour(uint16_t hour, bool hasUhr) {
    if (hasUhr && hour == 1) {
        showWord(WORD_EIN);
    } else {
        showWord(HOUR_WORDS[hour]);
    }
}

void showTimeLoop() {
    unsigned long currentTime = rtc.now();//*30;
    uint8_t minute = rtc.minute(currentTime);
    uint8_t hour = rtc.hour(currentTime);
    
    uint16_t minOfTheDay = hour * 60 + minute;
    if (lastMinOfTheDay != minOfTheDay) {
        lastMinOfTheDay = minOfTheDay;
        
		sun.setCurrentDate(rtc.year(currentTime), rtc.month(currentTime), rtc.day(currentTime));
		
		/* Check to see if we need to update our timezone value */
		if (rtc.isEuroDST(currentTime))
			sun.setTZOffset(TIMEZONE + 1);
		else
			sun.setTZOffset(TIMEZONE);
		
        uint16_t sunrise = sun.calcSunrise();
        uint16_t sunset = sun.calcSunset();

    // 	debug("Sunrise is %d", sunrise);
    // 	debug("Sunset is %d", sunset);

        // take a snapshot of the current state as source for fading
        nblend(src, dst, NUM_LEDS, fadeFract);
        fadeFract = 0;

        FastLED.clear();
        
        CRGB temperature;
        uint8_t brightness;
        
        if (minOfTheDay <= sunrise) {
            temperature = Candle;
            brightness = 105;
        } else if (minOfTheDay <= sunrise + 30) {
            temperature = blend(CRGB(Candle), CRGB(HighNoonSun), (minOfTheDay - sunrise) * 8);
            brightness = 105 + (minOfTheDay - sunrise) * 5;
        } else if (minOfTheDay <= sunset - 30) {
            temperature = CRGB(HighNoonSun);
            brightness = 255;
        } else if (minOfTheDay <= sunset) {
            temperature = blend(CRGB(Candle), CRGB(HighNoonSun), (sunset - minOfTheDay) * 8);
            brightness = 105 + (sunset - minOfTheDay) * 5;
        } else {
            temperature = Candle;
            brightness = 105;
        }
        // FastLED.setTemperature(temperature);
        // temperature.nscale8_video(brightness);
        
        for (uint8_t row = 0; row < 10; ++row) {        
            palette[row] = temperature;//CRGB::White;
        }
        
        // set minute points
        for (uint8_t dot = 0; dot != minute % 5; ++dot) {
            leds[dot] = palette[0];
        }
        
        if (lang.fullSentence) {
            showWord(WORD_ES);
            showWord(WORD_IST);
        }
        
        bool hasUhr = false;
        switch (minute / 5) {
            case 0:
                hasUhr = true;
                showWord(WORD_UHR);
                break;
            case 1:
                showWord(WORD_FUNF);
                showWord(WORD_NACH);
                break;
            case 2:
                showWord(WORD_ZEHN);
                showWord(WORD_NACH);
                break;
            case 3:
                showWord(WORD_VIERTEL);
                if (lang.dialect == LANG_OSSI) {
                    ++hour;
                } else {
                    showWord(WORD_NACH);
                }
                break;
            case 4:
                if (lang.dialect == LANG_RHEIN_RUHR) {
                    showWord(WORD_ZWANZIG);
                    showWord(WORD_NACH);
                } else {
                    showWord(WORD_ZEHN);
                    showWord(WORD_VOR);
                    showWord(WORD_HALB);
                    ++hour;
                }
                break;
            case 5:
                showWord(WORD_FUNF);
                showWord(WORD_VOR);
                showWord(WORD_HALB);
                ++hour;
                break;
            case 6:
                showWord(WORD_HALB);
                ++hour;
                break;
            case 7:
                showWord(WORD_FUNF);
                showWord(WORD_NACH);
                showWord(WORD_HALB);
                ++hour;
                break;
            case 8:
                if (lang.dialect == LANG_RHEIN_RUHR) {
                    showWord(WORD_ZWANZIG);
                    showWord(WORD_VOR);
                } else {
                    showWord(WORD_ZEHN);
                    showWord(WORD_NACH);
                    showWord(WORD_HALB);
                }
                ++hour;
                break;
            case 9:
                if (lang.dialect == LANG_OSSI) {
                    showWord(WORD_DREI);
                    showWord(WORD_VIERTEL);
                } else {
                    showWord(WORD_VIERTEL);
                    showWord(WORD_VOR);
                }
                ++hour;
                break;
            case 10:
                showWord(WORD_ZEHN);
                showWord(WORD_VOR);
                ++hour;
                break;
            case 11:
                showWord(WORD_FUNF);
                showWord(WORD_VOR);
                ++hour;
                break;
        }
        
        showHour(hour % 12, hasUhr);

        // The set state will the fading destination
        memcpy(dst, leds, NUM_LEDS * sizeof(CRGB));
    }
}

void fadeLoop() {
    bool updateLeds = true;

    if (fadeFract <= 255 - FADE_STEP) {
        fadeFract += FADE_STEP;
    } else if (fadeFract != 255) {
        fadeFract = 255;
    } else {
        updateLeds = false;
    }

    // for (uint8_t step = 0; brightness != targetBrightness && step < FADE_STEP; ++step) {
    if (brightness != targetBrightness) {
        updateLeds = true;
        if (brightness < targetBrightness) {
            ++brightness;
        } else {
            --brightness;
        }
    }

    if (updateLeds) {
        blend(src, dst, leds, NUM_LEDS, fadeFract);
        nscale8(leds, NUM_LEDS, brightness);
        napplyGamma_video(leds, NUM_LEDS, 2.5);
        FastLED.show();
    }
}



void setup() {
    theme.setColor(LED_SIGNAL_CLOUD_CONNECTED, 0x00000000); // Set LED_SIGNAL_NETWORK_ON to no color
    theme.apply(); // Apply theme settings

    FastLED.addLeds<PIXEL_TYPE, PIXEL_PIN>(leds, NUM_LEDS);//.setCorrection(TypicalSMD5050);
    // FastLED.setBrightness(127);
    
    FastLED.clear();
    FastLED.show();
    memcpy(dst, leds, NUM_LEDS * sizeof(CRGB));
    rtc.begin(&UDPClient, "0.de.pool.ntp.org");
    rtc.setTimeZone(TIMEZONE);
    rtc.setUseEuroDSTRule(true);
    sun.setPosition(LATITUDE, LONGITUDE, TIMEZONE);
    
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
    
    tsl2591.parameter.gain = 0b01;
    
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
    }


}

uint8_t sensorIdx = 0;

void loop() {
    EVERY_N_MILLISECONDS(20) {
        showTimeLoop();
        fadeLoop();
    }
    EVERY_N_MILLISECONDS(1000) {
        switch (sensorIdx) {
            case 0:
                temperature = bme280.readTempC();
                break;
            case 1:
                pressure = bme280.readPressure();
                break;
            case 2:
                humidity = bme280.readHumidity();
                break;
            case 3:
                illuminance = tsl2591.readIlluminance_TSL2591();
        }
        sensorIdx = (sensorIdx + 1) & 3;
        
        double logIlluminance = log(illuminance + 1) * 70 - 250;
        if (logIlluminance > 255) {
            targetBrightness = 255;
        } else if (logIlluminance < 0) {
            targetBrightness = 0;
        } else {
            targetBrightness = logIlluminance;
        }
        debug("target %d", targetBrightness);
        // Particle.publish("temperature", String(temperature), 60, PRIVATE);
        // Particle.publish("pressure", String(pressure), 60, PRIVATE);
        // Particle.publish("humidity", String(humidity), 60, PRIVATE);
        // Particle.publish("illuminance", String(illuminance), 60, PRIVATE);
    }
}