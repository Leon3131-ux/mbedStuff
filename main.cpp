#include "mbed.h"
#include "OLEDDisplay.h"
#include "HTS221Sensor.h"
#include <cstdio>
#include <string>
#include <math.h>
#include <LSM6DSLSensor.h>
#include "http_request.h"
#include "MFRC522.h"


OLEDDisplay oled( MBED_CONF_IOTKIT_OLED_RST, MBED_CONF_IOTKIT_OLED_SDA, MBED_CONF_IOTKIT_OLED_SCL );
DevI2C devI2c( MBED_CONF_IOTKIT_I2C_SDA, MBED_CONF_IOTKIT_I2C_SCL );
HTS221Sensor tempSensor(&devI2c);
DigitalIn button1( BUTTON1 );
MFRC522 rfidReader( MBED_CONF_IOTKIT_RFID_MOSI, MBED_CONF_IOTKIT_RFID_MISO, MBED_CONF_IOTKIT_RFID_SCLK, MBED_CONF_IOTKIT_RFID_SS, MBED_CONF_IOTKIT_RFID_RST ); 


static LSM6DSLSensor acc_gyro( &devI2c, LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW ); // low address
uint16_t tap_count = 0;


// main() runs in its own thread in the OS
int main()
{

    float temp;
    float hum;
    int button_count = 0;
    char body[2048];
    rfidReader.PCD_Init();



    uint8_t id;
    LSM6DSL_Event_Status_t status;
    acc_gyro.init(NULL);
    acc_gyro.enable_x();    
    acc_gyro.enable_g();
    acc_gyro.enable_single_tap_detection();

    tempSensor.init(NULL);
    tempSensor.enable();

    acc_gyro.read_id(&id);

    WiFiInterface* network = WiFiInterface::get_default_instance();
    if (!network) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = network->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", network->get_mac_address());
    SocketAddress a;
    network->get_ip_address(&a);
    printf("IP: %s\n", a.get_ip_address());

    while (true) {

        tempSensor.get_temperature(&temp);
        tempSensor.get_humidity(&hum);

        if ( rfidReader.PICC_IsNewCardPresent())
            if ( rfidReader.PICC_ReadCardSerial()) 
            {
                HttpRequest* post_req_rfid = new HttpRequest( network, HTTP_POST, "http://164.92.173.232:23552/api/sensor/data");
                int piccType = rfidReader.PICC_GetType(rfidReader.uid.sak);
                post_req_rfid->set_header("content-type", "application/json");
                sprintf( body, "[{\"type\": \"RFID\", \"value\": \"%%02X:%02X:%02X:%02X\"}]", rfidReader.uid.uidByte[0], rfidReader.uid.uidByte[1], rfidReader.uid.uidByte[2], rfidReader.uid.uidByte[3]);
                HttpResponse* post_res_rfid = post_req_rfid->send(body, strlen(body));
                delete post_req_rfid;
            }
        thread_sleep_for( 200 );

        acc_gyro.get_event_status( &status );
        if  ( status.TapStatus ) {
            tap_count++;
            oled.cursor( 1, 0 );
            printf( "tap %6d\n", tap_count );
        }
        thread_sleep_for( 100 );

        
        if  ( button1 == 0 ) 
        {
            button_count++;
            printf( "BTN %6d\n", button_count );
        }
        
        HttpRequest* post_req = new HttpRequest( network, HTTP_POST, "http://164.92.173.232:23552/api/sensor/data");
        post_req->set_header("content-type", "application/json");
        sprintf( body, "[{\"type\": \"TEMP\", \"value\": \"%f\"}, {\"type\": \"HUM\", \"value\": \"%f\"}, {\"type\": \"BEATC\", \"value\": \"%i\"}, {\"type\": \"BTNC\", \"value\": \"%i\"}]", temp, hum, tap_count, button_count);

        thread_sleep_for( 1000 );
        HttpResponse* post_res = post_req->send(body, strlen(body));
        delete post_req;


        oled.clear();
        oled.printf("current temp: %.2f\ncurrent hum: %.2f\nCurrent tap: %i\nCurrent btn count: %i", temp, hum, tap_count, button_count);
    }
}
