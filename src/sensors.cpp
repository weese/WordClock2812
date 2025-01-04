// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>

#include "config.h"
#include <widgets/settings_general.h>

Thread* measureWorker;
extern int targetBrightness;



#ifdef LUMINANCE_SENSOR_ANALOG

uint32_t lumSum = 0;
uint8_t lumCount = 0;
int illuminance = 0;

os_thread_return_t measureLoop(void* param) {
    while (true) {
        // compute new brightness
        lumSum += analogRead(LUMINANCE_SENSOR_PIN);
        if (++lumCount == 64) {
            int newLuminance = lumSum / 64;
            int delta = newLuminance - illuminance;
            if (delta < -LUM_THRESH || delta > LUM_THRESH || newLuminance <= MIN_LUMINANCE || newLuminance >= MAX_LUMINANCE)
                illuminance = newLuminance;
            lumSum = 0;
            lumCount = 0;
        }
        targetBrightness = lum2brightness(illuminance);
        os_thread_yield();
    }
}

void setupSensors() {
    Particle.variable("illuminance", illuminance);
    measureWorker = new Thread(NULL,  measureLoop);
}
#endif




#ifdef LUMINANCE_SENSOR_BME280_TSL2591

#define RwReg

#include <Wire.h>
#include <BlueDot_BME280_TSL2591.h>

BlueDot_BME280_TSL2591 bme280;
BlueDot_BME280_TSL2591 tsl2591;

uint8_t sensorIdx = 0;
double illuminance = 0;
double temperature = 0;
double pressure = 0;
double humidity = 0;

extern Mutex systemLock;

uint8_t getBrightness() {
    illuminance = tsl2591.readIlluminance_TSL2591();
    return lum2brightness((int)(log(illuminance + 1) * 50));
}

#ifdef LOSANT_BROKER
MQTT client(LOSANT_BROKER, 1883, NULL);

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

    // Send the json payload:
    // { “data” : { “temperature” : val, "humidity": val, “pressure” : val, “illuminance” : val }}

    systemLock.lock();
    client.loop();
    client.publish(MQTT_TOPIC_STATE, String::format(
        "{\"data\":{\"temperature\":%f,\"humidity\":%f,\"pressure\":%f,\"illuminance\":%f}}",
        temperature,
        humidity,
        pressure,
        illuminance));
    systemLock.unlock();
}
#else

void loopLosant() {}

#endif

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
            if (++sensorIdx >= 3) {
                sensorIdx = 0;
            }
            
            // Particle.publish("temperature", String(temperature), 60, PRIVATE);
            // Particle.publish("pressure", String(pressure), 60, PRIVATE);
            // Particle.publish("humidity", String(humidity), 60, PRIVATE);
            // Particle.publish("illuminance", String(illuminance), 60, PRIVATE);
        }
        os_thread_yield();
    }
}

void setupSensors() {
    Wire.setSpeed(400000);
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

    if (bme280.init_BME280() == 0x60) {
        Particle.variable("temperature", temperature);
        Particle.variable("pressure", pressure);
        Particle.variable("humidity", humidity);
    }
    
    if (tsl2591.init_TSL2591() == 0x50) {
        Particle.variable("illuminance", illuminance);
    }

    measureWorker = new Thread(NULL,  measureLoop);
}
#endif
