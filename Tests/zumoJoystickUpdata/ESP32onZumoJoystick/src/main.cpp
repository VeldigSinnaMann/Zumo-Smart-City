// esp på zumo
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>

#define I2C_SLAVE_ADDRESS 4

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "NTNU-IOT";
const char *password = "";
const char *mqtt_server = "10.25.17.1";  // rpi ip adresse
const char *mqtt_topic_steeringWheel = "zumo/kontrollerInput";
const char *mqtt_topic_drivingPattern = "esp32/kjoreMonster";

char licencePlate[] = "0001";
long lastMsg = 0;
char msg[50];
int value = 0;
unsigned long lastReconnectTime = 0;
unsigned long lastRequestTime = 0;

// Connect to Wifi
void setup_wifi() {
  delay(10);
  // kolbe på wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {  // viser prikker til kobling er god
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Reconnect mqtt
void reconnect() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_steeringWheel);  // Abonner på MQTT topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Convert frol float to byte-array
union FloatConverter {
  float value;
  uint8_t bytes[sizeof(float)];
};

// Send data to zumo for steering
void send_i2c_data(int16_t leftMotor, int16_t rightMotor) {
  Serial.println("BeginTransmisson");
  Serial.println(rightMotor);
  Serial.println(leftMotor);

  Wire.beginTransmission(I2C_SLAVE_ADDRESS);
  Wire.write(lowByte(leftMotor));
  Wire.write(highByte(leftMotor));
  Wire.write(lowByte(rightMotor));
  Wire.write(highByte(rightMotor));
  uint8_t result = Wire.endTransmission();
  Serial.print("result:");
  Serial.println(result);

  if (result == 0) {
    Serial.print("Sent via I2C - Left Motor: ");
    Serial.print(leftMotor);
    Serial.print(", Right Motor: ");
    Serial.println(rightMotor);
  } else {
    Serial.print("Error sending I2C data: ");
    Serial.println(result);
    Serial.println("sko");
    // Add appropriate error handling here, e.g., retry logic
  }
}

// Check for driving pattern data from zumo to send to mqtt server
void check_for_i2c_data() {
  Serial.println("ceck_for_i2c_data");
  Wire.requestFrom(I2C_SLAVE_ADDRESS, sizeof(float));
  Serial.print("wire avalieable: ");
  Serial.println(Wire.available());
  if (Wire.available() == sizeof(float)) {
    Serial.println("Wire avaliable");
    FloatConverter converter;
    for (int i = 0; i < sizeof(float); i++) {
      converter.bytes[i] = Wire.read();
    }
    Serial.print("Recieved kjøremønsterdata fra Zumo: ");
    Serial.println(converter.value, 3);
    char sendValue[20];
    dtostrf(converter.value, 1, 3, sendValue);
    strcat(sendValue, ", ");
    strcat(sendValue, licencePlate);
    client.publish(mqtt_topic_drivingPattern, sendValue);
  }
}

// Recieve data from joystick-Esp over I2C and send to zumo for steering
void callback(char *topic, byte *payload, unsigned int length) {
  if(String(topic) = "esp32/kontrollerOutput"){
  Serial.println("callback");
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  // Decompose payload to usable values
  char *saveptr;
  int16_t leftMotor = atoi(strtok_r(message, ",", &saveptr));
  int16_t rightMotor = atoi(strtok_r(NULL, ",", &saveptr));

  // Ensure values are within range
  leftMotor = constrain(leftMotor, 0, 800);
  rightMotor = constrain(rightMotor, 0, 800);

  // Now send these values with I2C
  send_i2c_data(leftMotor, rightMotor);
  }
}

void setup() {
  Wire.begin();  // Setting ESP32 as master
  Wire.setClock(100000);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);  // Connect to mqtt server
  client.setCallback(callback);         // Run callback to drive car
}

void loop() {
  Serial.println("alive");
  if (!client.connected()) {
    unsigned long CurrentReconnectTime = millis();
    if (CurrentReconnectTime - lastReconnectTime >= 5000) {
      lastReconnectTime = CurrentReconnectTime;
      reconnect();
    }
  }

  client.loop();
  
  if (millis() - lastRequestTime > 1000) {
    //check_for_i2c_data();
    lastRequestTime = millis();
  }
}