#include "mbed.h"
#include "OLEDDisplay.h"
#include "HTS221Sensor.h"
#include <cstdio>
#include <string>
#include <math.h>
#include <LSM6DSLSensor.h>
#include "MFRC522.h"
#include "Motor.h"

#include <MQTTClientMbedOs.h>
#include <MQTTNetwork.h>
#include <MQTTClient.h>
#include <MQTTmbed.h>

#if MBED_CONF_IOTKIT_BMP180_SENSOR == true
#include "BMP180Wrapper.h"
#endif

#ifdef TARGET_K64F
#include "QEI.h"

//Use X2 encoding by default.
QEI wheel (MBED_CONF_IOTKIT_BUTTON2, MBED_CONF_IOTKIT_BUTTON3, NC, 624);
#endif

// Topic's publish
char* topicTEMP = (char*) "iotkit/temp";
char* topicHUM = (char*) "iotkit/humidity";
char* topicBUTTON = (char*) "iotkit/button";
char* topicGYRO = (char*) "iotkit/gyro";
char* topicRFID = (char*) "iotkit/rfid";
// Topic's subscribe
char* topicActors = (char*) "iotkit/display";
// MQTT Brocker
char* hostname = (char*) "164.92.173.232";
int port = 1883;
// MQTT Message
MQTT::Message message;
// I/O Buffer
char buf[100];

// Klassifikation 
char cls[3][10] = { "low", "middle", "high" };
int type = 0;


OLEDDisplay oled( MBED_CONF_IOTKIT_OLED_RST, MBED_CONF_IOTKIT_OLED_SDA, MBED_CONF_IOTKIT_OLED_SCL );
DevI2C devI2c( MBED_CONF_IOTKIT_I2C_SDA, MBED_CONF_IOTKIT_I2C_SCL );
HTS221Sensor tempSensor(&devI2c);
DigitalIn button1( BUTTON1 );
MFRC522 rfidReader( MBED_CONF_IOTKIT_RFID_MOSI, MBED_CONF_IOTKIT_RFID_MISO, MBED_CONF_IOTKIT_RFID_SCLK, MBED_CONF_IOTKIT_RFID_SS, MBED_CONF_IOTKIT_RFID_RST ); 

Motor m1( MBED_CONF_IOTKIT_MOTOR2_PWM, MBED_CONF_IOTKIT_MOTOR2_FWD, MBED_CONF_IOTKIT_MOTOR2_REV ); // PWM, Vorwaerts, Rueckwarts

static LSM6DSLSensor acc_gyro( &devI2c, LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW ); // low address
uint16_t tap_count = 0;

void publish( MQTTNetwork &mqttNetwork, MQTT::Client<MQTTNetwork, Countdown> &client, char* topic )
{
    MQTT::Message message;    
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buf;
    message.payloadlen = strlen(buf)+1;
    client.publish( topic, message);  
}   

/** Daten empfangen von MQTT Broker */
void messageArrived( MQTT::MessageData& md )
{
    string value;
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    printf("Topic %.*s, ", md.topicName.lenstring.len, (char*) md.topicName.lenstring.data );
    // MQTT schickt kein \0, deshalb manuell anfuegen
    ((char*) message.payload)[message.payloadlen] = '\0';
    printf("Payload %s\n", (char*) message.payload);

    // Aktoren
    if  ( strncmp( (char*) md.topicName.lenstring.data + md.topicName.lenstring.len - 7 , "display", 7) == 0 )
    {
        sscanf( (char*) message.payload, "%s*", &value );
        printf( "Display %s\n", &value );
        oled.clear();
        oled.printf("%s", &value);
        thread_sleep_for( 10000 );
    }               
}

// main() runs in its own thread in the OS
int main()
{

    float temp;
    float hum;
    int button_count = 0;
    int encoder;
    string display;
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

    WiFiInterface* wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }   

    // TCP/IP und MQTT initialisieren (muss in main erfolgen)
    MQTTNetwork mqttNetwork( wifi );
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc); 

    // Zugangsdaten - der Mosquitto Broker ignoriert diese
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = (char*) wifi->get_mac_address(); // muss Eindeutig sein, ansonsten ist nur 1ne Connection moeglich
    data.username.cstring = (char*) wifi->get_mac_address(); // User und Password ohne Funktion
    data.password.cstring = (char*) "password";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);           

    // MQTT Subscribe!
    client.subscribe( topicActors, MQTT::QOS0, messageArrived );
    printf("MQTT subscribe %s\n", topicActors );

    while (true) {

        tempSensor.read_id(&id);
        tempSensor.get_temperature(&temp);
        sprintf( buf, "%.2f", temp);
        publish( mqttNetwork, client, topicTEMP );

        tempSensor.read_id(&id);
        tempSensor.get_humidity(&hum);  
        sprintf( buf, "%.2f", hum);
        publish( mqttNetwork, client, topicHUM );


        if ( rfidReader.PICC_IsNewCardPresent())
            if ( rfidReader.PICC_ReadCardSerial()) 
            {
                // Print Card UID (2-stellig mit Vornullen, Hexadecimal)
                printf("Card UID: ");
                for ( int i = 0; i < rfidReader.uid.size; i++ )
                    printf("%02X:", rfidReader.uid.uidByte[i]);
                printf("\n");
                
                // Print Card type
                int piccType = rfidReader.PICC_GetType(rfidReader.uid.sak);

                printf("PICC Type: %s \n", rfidReader.PICC_GetTypeName(piccType) );
                
                sprintf( buf, "%02X:%02X:%02X:%02X:", rfidReader.uid.uidByte[0], rfidReader.uid.uidByte[1], rfidReader.uid.uidByte[2], rfidReader.uid.uidByte[3] );
                publish( mqttNetwork, client, topicRFID );                
                

            }

        acc_gyro.get_event_status( &status );
        if  ( status.TapStatus ) {
            tap_count++;
            oled.cursor( 1, 0 );
            printf(buf, "%d", tap_count);
            sprintf(buf, "%d", tap_count);
            publish( mqttNetwork, client, topicGYRO );
        } 

        
        if  ( button1 == 0 ) 
        {
            button_count++;
            printf( "BTN %6d\n", button_count );
            sprintf(buf, "%d", button_count);
            publish( mqttNetwork, client, topicBUTTON );
        }       

        oled.clear();
        oled.printf("current temp: %.2f\ncurrent hum: %.2f\nCurrent tap: %i\nCurrent btn count: %i", temp, hum, tap_count, button_count);
        

        client.yield    ( 100 );                   
        thread_sleep_for( 500 );
    }

}
