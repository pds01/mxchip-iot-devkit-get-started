// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 

#include "HTS221Sensor.h"
#include "AzureIotHub.h"
#include "Arduino.h"
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"
#include "Sensor.h"
#include "SystemTime.h"

#define RGB_LED_BRIGHTNESS 32

DevI2C *i2c;
HTS221Sensor *sensor;
LPS22HBSensor *lp_sensor;

static RGB_LED rgbLed;
static int interval = INTERVAL;
static float humidity;
static float temperature;
static float preasure;
static float soil;

int getInterval()
{
    return interval;
}

void blinkLED()
{
    rgbLed.turnOff();
    rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
    delay(500);
    rgbLed.turnOff();
}

void blinkSendConfirmation()
{
    rgbLed.turnOff();
    rgbLed.setColor(0, 0, RGB_LED_BRIGHTNESS);
    delay(500);
    rgbLed.turnOff();
}

void parseTwinMessage(DEVICE_TWIN_UPDATE_STATE updateState, const char *message)
{
    JSON_Value *root_value;
    root_value = json_parse_string(message);
    if (json_value_get_type(root_value) != JSONObject)
    {
        if (root_value != NULL)
        {
            json_value_free(root_value);
        }
        LogError("parse %s failed", message);
        return;
    }
    JSON_Object *root_object = json_value_get_object(root_value);

    double val = 0;
    if (updateState == DEVICE_TWIN_UPDATE_COMPLETE)
    {
        JSON_Object *desired_object = json_object_get_object(root_object, "desired");
        if (desired_object != NULL)
        {
            val = json_object_get_number(desired_object, "interval");
        }
    }
    else
    {
        val = json_object_get_number(root_object, "interval");
    }
    if (val > 500)
    {
        interval = (int)val;
        LogInfo(">>>Device twin updated: set interval to %d", interval);
    }
    json_value_free(root_value);
}

void SensorInit()
{
    i2c = new DevI2C(D14, D15);
    sensor = new HTS221Sensor(*i2c);
    sensor->init(NULL);
    lp_sensor= new LPS22HBSensor(*i2c);
    lp_sensor->init(NULL);
    preasure = -1;
    humidity = -1;
    soil = -1;
    temperature = -1000;
}

float readTemperature()
{
    sensor->reset();

    float temperature = 0;
    sensor->getTemperature(&temperature);

    return temperature;
}

float readPreasure()
{
    //lp_sensor->reset();

    float preasure = 0;
    lp_sensor->getTemperature(&preasure);

    return preasure;
}

float readHumidity()
{
    sensor->reset();

    float humidity = 0;
    sensor->getHumidity(&humidity);

    return humidity;
}

float readSoil()
{
    int sensorPin = 5;

    float soil = 0;
    
    soil = analogRead(sensorPin);

    return soil;
}

bool readMessage(int messageId, char *payload, float *temperatureValue, float *humidityValue, float *preasureValue, float *soilValue)
{
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    char *serialized_string = NULL;

    json_object_set_number(root_object, "messageId", messageId);

    float t = readTemperature();
    float h = readHumidity();
    float p = readPreasure();
    float s = readSoil();
    bool temperatureAlert = false;
    if(t != temperature)
    {
        temperature = t;
        *temperatureValue = t;
        json_object_set_number(root_object, "temperature", temperature);
    }
    if(temperature > TEMPERATURE_ALERT)
    {
        temperatureAlert = true;
    }
    
    if(h != humidity)
    {
        humidity = h;
        *humidityValue = h;
        json_object_set_number(root_object, "humidity", humidity);
    }

    if(s != soil)
    {
        soil = s;
        *soilValue = s;
        json_object_set_number(root_object, "soil", soil);
    }

    if(p != preasure)
    {
        preasure = p;
        *preasureValue = p;
        json_object_set_number(root_object, "preasure", preasure);
    }

    time_t st = time(NULL);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&st));


    json_object_set_string(root_object, "deviceId", DEVICE_ID);
    json_object_set_string(root_object, "messageId", buf);
     
    serialized_string = json_serialize_to_string_pretty(root_value);

    snprintf(payload, MESSAGE_MAX_LEN, "%s", serialized_string);
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
    return temperatureAlert;
}

#if (DEVKIT_SDK_VERSION >= 10602)
void __sys_setup(void)
{
    // Enable Web Server for system configuration when system enter AP mode
    EnableSystemWeb(WEB_SETTING_IOT_DEVICE_CONN_STRING);
}
#endif