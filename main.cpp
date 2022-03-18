#include "mbed.h"
#include "OLEDDisplay.h"
#include "HTS221Sensor.h"
#include <cstdio>
#include <string>
#include <math.h>


OLEDDisplay oled( MBED_CONF_IOTKIT_OLED_RST, MBED_CONF_IOTKIT_OLED_SDA, MBED_CONF_IOTKIT_OLED_SCL );
DevI2C devI2c( MBED_CONF_IOTKIT_I2C_SDA, MBED_CONF_IOTKIT_I2C_SCL );
HTS221Sensor tempSensor(&devI2c);

// main() runs in its own thread in the OS
int main()
{

    float temp;
    float hum;

    tempSensor.init(NULL);
    tempSensor.enable();

    while (true) {

        tempSensor.get_temperature(&temp);
        tempSensor.get_humidity(&hum);

        oled.clear();
        oled.printf("current temp: %.2f\ncurrent hum: %.2f\n", temp, hum);
    }

    // Connect to the network with the default networking interface
    // if you use WiFi: see mbed_app.json for the credentials
    // WiFiInterface* network = WiFiInterface::get_default_instance();
    // if (!network) {
    //     printf("ERROR: No WiFiInterface found.\n");
    //     return -1;
    // }

    // printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    // int ret = network->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    // if (ret != 0) {
    //     printf("\nConnection error: %d\n", ret);
    //     return -1;
    // }
    // printf("Success\n\n");
    // printf("MAC: %s\n", network->get_mac_address());
    // SocketAddress a;
    // network->get_ip_address(&a);
    // printf("IP: %s\n", a.get_ip_address());


}

