#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#define MQTT_KEEPALIVE 5

#include <Arduino.h>
#include <IRremoteESP8266.h>
//#include <IRsend.h>
#include <ir_Gree.h>
#include <Wire.h>
#include "SparkFun_Si7021_Breakout_Library.h"

const char* ssid = "YOUR SSID";
const char* password = "YOUR PASSWORD";
const char* mqtt_server = "YOUR MQTT SERVER IP ADDRESS";

char out_str[50];

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

float average_temp = 25.0;

// Set-values from MQTT
volatile int ac_set_temp = 25;
volatile int ac_diff_temp = 0;
volatile bool ac_on = true;
volatile int ac_mode = 1; // kGreeAuto = 0 kGreeCool = 1; kGreeDry = 2; kGreeFan = 3; kGreeHeat = 4;
volatile bool ac_ifeel = false;
volatile bool ac_light = true;
volatile int ac_swing = 0;
volatile int ac_fan = 1;

volatile bool last_ac_on = true;
volatile int last_ac_mode = 1; // kGreeAuto = 0 kGreeCool = 1; kGreeDry = 2; kGreeFan = 3; kGreeHeat = 4;
volatile bool last_ac_ifeel = false;
volatile bool last_ac_light = true;
volatile int last_ac_swing = 0;
volatile int last_ac_fan = 1;

volatile int last_set_temp = 0;

volatile bool should_update_temp = false;
void onConnectionEstablished();

volatile float humidity = 0;
volatile float temp = 0;

Weather sensor;

#define SEND_GREE

const uint16_t kIrLed = D0;  

IRGreeAC irgree(kIrLed, YAW1F, false, true);  // Set the GPIO to be used to sending the message.
#define SDA_PIN D2
#define SCL_PIN D1

#define AVERAGE_LENGTH 16

// These are *10 - so 25.2 degrees are 252
int average_temps[AVERAGE_LENGTH];
int averate_temps_index = 0;

void setup_averages(void) {
    // Set up  average 
  humidity = sensor.getRH();
  temp = sensor.readTemp();
  int i;
  for (i=0;i<AVERAGE_LENGTH;i++) {
    average_temps[i] = temp*10;
  }
}

void add_to_averages(float temp) {
  averate_temps_index++;
  if (averate_temps_index>=AVERAGE_LENGTH) averate_temps_index = 0;
  average_temps[averate_temps_index] = temp*10;
}

void update_average_temp(void) {
  int sum = 0;
  int i;
  for(i=0; i<AVERAGE_LENGTH; i++) sum+= average_temps[i];
  average_temp = 0.1*sum/AVERAGE_LENGTH;
    Serial.print("AverageTemp:");
  Serial.print(average_temp);
  Serial.println("C");

  sprintf(out_str, "%02f", average_temp);
  client.publish("Tadiran_1/AverageTemp", out_str);

}


Ticker MeasurementTicker;
Ticker UpdateTicker;

void measurement_isr() {
  add_to_averages(temp);
  Serial.print("Temp:");
  Serial.print(temp);
  Serial.print("C, ");
  sprintf(out_str, "%02f", temp);
  client.publish("Tadiran_1/RoomTemp", out_str);

  Serial.print("Humidity:");
  Serial.print(humidity);
  Serial.println("%");
  sprintf(out_str, "%02f", humidity);
  client.publish("Tadiran_1/RoomHumidity", out_str);

}

void update_ac(void) {
  should_update_temp = false;
  int new_temp;
  update_average_temp();

  ac_diff_temp = int(ac_set_temp - average_temp);
  if (ac_diff_temp > 5) ac_diff_temp = 5;
  if (ac_diff_temp < -5) ac_diff_temp = -5;
      sprintf(out_str, "%02d", ac_diff_temp);
      client.publish("Tadiran_1/DiffTemp", out_str);
  Serial.print("DiffTemp:");
  Serial.println(ac_diff_temp);

  new_temp = ac_set_temp + ac_diff_temp;
  if (new_temp < 16) new_temp = 16;
  if (new_temp > 30) new_temp = 30;
    Serial.print("last_set_temp:");
  Serial.print(last_set_temp);
    Serial.print("new_temp:");
  Serial.println(new_temp);
Serial.print("lst!=new");
Serial.println(last_set_temp != new_temp);
Serial.print("ac_on");
Serial.println(ac_on == true);
  if ((last_set_temp != new_temp) && (ac_on == true)) {
      last_set_temp = new_temp;
      Serial.print("TempUpdate");
      Serial.println(new_temp);
      sprintf(out_str, "%02d", new_temp);
      client.publish("Tadiran_1/TempUpdate", out_str);
    irgree.setTemp(new_temp);
    irgree.send(2);
  }
}

void update_isr() {
should_update_temp = true;
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char* ptopic;
  ptopic = (char*) malloc(length + 1);
  strncpy(ptopic, (char*)payload, length);
  ptopic[length] = '\0';
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, "Tadiran_1/SetTemp") == 0) {
    int val;
    val = atoi(ptopic);
    ac_set_temp = val;
  }
  if (strcmp(topic, "Tadiran_1/SetOn") == 0) {
    int val;
    val = atoi(ptopic);
    ac_on = val == 1;
    if (ac_on) {
        irgree.on();
    } else {
        irgree.off();
    }
    if (last_ac_on != ac_on) {
      last_ac_on = ac_on;
        irgree.send(2);
    }
  }
  if (strcmp(topic, "Tadiran_1/SetMode") == 0) {
    ac_mode = 1;
    if (strcmp(ptopic, "auto") == 0) ac_mode = 0;
    if (strcmp(ptopic, "cool") == 0) ac_mode = 1;
    if (strcmp(ptopic, "dry") == 0) ac_mode = 2;
    if (strcmp(ptopic, "fan_only") == 0) ac_mode = 3;
    if (strcmp(ptopic, "heat") == 0) ac_mode = 4;
    irgree.setMode(ac_mode);
    if (last_ac_mode != ac_mode) {
      irgree.send(2);
      last_ac_mode = ac_mode;
    }
  }
  if (strcmp(topic, "Tadiran_1/SetIFeel") == 0) {
    int val;
    val = atoi(ptopic);
    ac_ifeel = val == 1;
    irgree.setIFeel(ac_ifeel);
    if (last_ac_ifeel != ac_ifeel) {
      irgree.send(2);
      last_ac_ifeel = ac_ifeel;
    }
  }
  if (strcmp(topic, "Tadiran_1/SetLight") == 0) {
    int val;
    val = atoi(ptopic);
    ac_light = val == 1;
    irgree.setLight(ac_light);
    if (last_ac_light != ac_light) {
      last_ac_light = ac_light;
      irgree.send(2);
    }
  }
  if (strcmp(topic, "Tadiran_1/SetSwing") == 0) {
    ac_swing = 1;
    if (strcmp(ptopic, "LastPos") == 0) ac_swing = 0;
    if (strcmp(ptopic, "Auto") == 0) ac_swing = 1;
    if (strcmp(ptopic, "Up") == 0) ac_swing = 2;
    if (strcmp(ptopic, "MiddleUp") == 0) ac_swing = 3;
    if (strcmp(ptopic, "Middle") == 0) ac_swing = 4;
    if (strcmp(ptopic, "MiddleDown") == 0) ac_swing = 5;
    if (strcmp(ptopic, "Down") == 0) ac_swing = 6;
    if (strcmp(ptopic, "DownAuto") == 0) ac_swing = 7;
    if (strcmp(ptopic, "MiddleAuto") == 0) ac_swing = 9;
    if (strcmp(ptopic, "UpAuto") == 0) ac_swing = 11;

//const uint8_t kGreeSwingLastPos =    0b00000000; 0
//const uint8_t kGreeSwingAuto =       0b00000001; 1
//const uint8_t kGreeSwingUp =         0b00000010; 2
//const uint8_t kGreeSwingMiddleUp =   0b00000011; 3
//const uint8_t kGreeSwingMiddle =     0b00000100; 4
//const uint8_t kGreeSwingMiddleDown = 0b00000101; 5
//const uint8_t kGreeSwingDown =       0b00000110; 6
//const uint8_t kGreeSwingDownAuto =   0b00000111; 7
//const uint8_t kGreeSwingMiddleAuto = 0b00001001; 9 
//const uint8_t kGreeSwingUpAuto =     0b00001011; 11
    if ((ac_swing == 1) || (ac_swing == 7) || (ac_swing == 9) || (ac_swing == 11)) { 
          irgree.setSwingVertical(true, ac_swing);
      } else { 
          irgree.setSwingVertical(false, ac_swing);
        }
    if (last_ac_swing != ac_swing) {
      last_ac_swing = ac_swing;
      irgree.send(2);
    }
  }
  if (strcmp(topic, "Tadiran_1/SetFan") == 0) {
    int val;
    val = 1;
    if (strcmp(ptopic, "auto") == 0) val = 0;
    if (strcmp(ptopic, "high") == 0) val = 3;
    if (strcmp(ptopic, "medium") == 0) val = 2;
    if (strcmp(ptopic, "low") == 0) val = 1;
    ac_fan = val;
    irgree.setFan(ac_fan);// Set the speed of the fan, 0-3, 0 is auto, 1-3 is the speed
    if (last_ac_fan != ac_fan) {
      last_ac_fan = ac_fan;
    irgree.send(2);
    }
  }
  free(ptopic);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "Tadiran_1";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("Tadiran_1/SetTemp");
      client.subscribe("Tadiran_1/SetOn");
      client.subscribe("Tadiran_1/SetMode");
      client.subscribe("Tadiran_1/SetIFeel");
      client.subscribe("Tadiran_1/SetLight");
      client.subscribe("Tadiran_1/SetSwing");
      client.subscribe("Tadiran_1/SetFan");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
}


void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);

#if ESP8266
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else  // ESP8266
  Serial.begin(115200, SERIAL_8N1);
#endif  // ESP8266

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  irgree.begin();
  irgree.stateReset();
  irgree.on();
  irgree.setMode(kGreeCool); // kGreeCool kGreeHeat kGreeDry
  irgree.setIFeel(false);
  irgree.setLight(true);
  irgree.setSwingVertical(false, kGreeSwingMiddleUp); // kGreeSwingUp kGreeSwingMiddleUp kGreeSwingMiddleUp kGreeSwingMiddleDown kGreeSwingDown 
 // irgree.setSwingVertical(true, kGreeSwingUp); //  kGreeSwingAuto kGreeSwingDownAuto kGreeSwingMiddleAuto kGreeSwingUpAuto 
  irgree.setFan(1);// Set the speed of the fan, 0-3, 0 is auto, 1-3 is the speed
  irgree.setTemp(25);

  setup_averages();
  
  MeasurementTicker.attach_ms(5000, measurement_isr);
  UpdateTicker.attach_ms(60000, update_isr);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  humidity = sensor.getRH();
  temp = sensor.readTemp();
    delay(500);
  if (should_update_temp == true) update_ac();
}
