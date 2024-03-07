#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Preferences.h>

/* Zde můžete měnit hodnoty lux.
   minvalueoflux znamena minimalni hodnota lux v mistnosti pri ktere bude svetylko aspon svitit
   maxvalueoflux znamena ze pokud bude lux vetsi nez horni hranice tak bude svitit jako by svetlo byla presne horni hranice
   takze pokud nastavite minvalueoflux na 0 a maxvalueoflux na 255 a svitivost v mistnosti bude 127 tak svetlo bude svitit stredne
   tyto hodnoty se meni pres mqtt, kdyz je zmenite tady tak to nebude mit vliv protoze se hodnoty stejne berou z pameti NVS
*/
int minvalueoflux = 0;
int maxvalueoflux = 255;

// Wi-Fi připojení
const char* ssid = "jmenowifi"; //zde zadejte jmeno vasi wifi
const char* password = "heslowifi"; //zde zadejte heslo od vasi wifi


// MQTT nastavení
const char* mqttServer = "broker.hivemq.com"; //zde můžete použít různé servery, např. test.mosquitto.org, broker.emqx.io, broker.hivemq.com
const int mqttPort = 1883;
const char* mqttUser = nullptr;
const char* mqttPassword = nullptr;
const char* mqttTopic = "seeluxbystepan";  //topic pro posilani hodnot ze senzoru
const char* mqttTopic1 = "changeminluxbystepan"; //topic pro zmenu minvalueoflux
const char* mqttTopic2 = "changemaxluxbystepan"; //topic pro zmenu maxvalueoflux


const int lightPin = 13; // zde muzete menit GPIO pin pro napájení LED diody (default 13 - IO13)

BH1750 lightMeter;
WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// Funkce pro ovládání LED na základě hodnoty světla
void controlLED(float lux) {
  if (lux < minvalueoflux) {
    lux = minvalueoflux;//pokud je pod tak to dej jako min = nesviti vubec
  } else if (lux > maxvalueoflux) {
    lux = maxvalueoflux;//pokud presahuje tak to dej jako max = sviti nejvic
  }
  int pwmsignal = int((lux - minvalueoflux) / (maxvalueoflux - minvalueoflux) * 255); //vypocet pro to jak moc ma svetlo svitit
  analogWrite(lightPin, pwmsignal); // Převod napětí na hodnotu pro PWM
}

// Funkce pro výpis hodnoty světla
void printLux(float lux){
  Serial.print("Svetlo: ");
  Serial.print(lux); 
  Serial.println(" lux");
}

// Funkce pro nastavení Wi-Fi připojení
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Pripojovani k WiFi...");
  WiFi.begin(ssid, password);//zkus se pripojit na wifi jmenem ssid a s heslem password
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wifi pripojeno!");
}

// Callback funkce pro zpracování zpráv přijatých přes MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Uzavření řetězce
  String receivedMessage = String((char*)payload);

  // Kontrola témata a provedení akce
  if (strcmp(topic, mqttTopic1) == 0) {
    // Změna minimální hodnoty lux
    minvalueoflux = receivedMessage.toInt();
    preferences.putInt("minvalueoflux", minvalueoflux);//ulozeni do NVS pameti
    Serial.print("Zmena minlux: ");
    Serial.println(minvalueoflux);
  } else if (strcmp(topic, mqttTopic2) == 0) {
    // Změna maximální hodnoty lux
    maxvalueoflux = receivedMessage.toInt();
    preferences.putInt("maxvalueoflux", maxvalueoflux);//ulozeni do NVS pameti
    Serial.print("Zmena maxlux: ");
    Serial.println(maxvalueoflux);
  }
}

//pripojeni k MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Pripojovani k MQTT...");
    if (client.connect("stepanajehosvetlokteresemeni", mqttUser, mqttPassword)) {
      Serial.println("Pripojeno!");
      client.subscribe(mqttTopic1);//odber topicu pro zmenu minvalueoflux
      client.subscribe(mqttTopic2);//odber topicu pro zmenu maxvalueoflux
    } else {
      Serial.print("chyba, rc=");
      Serial.print(client.state());
      Serial.println(" zkousim znovu za 5 sekund");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  preferences.begin("stepanaplikace", false);  // Zacatek používání NVS pameti s nazvem "stepanaplikace"

  // Načtení uložených hodnot nebo nastavení výchozích hodnot
  minvalueoflux = preferences.getInt("minvalueoflux", 0);
  maxvalueoflux = preferences.getInt("maxvalueoflux", 255);


  Wire.begin(); // Inicializace I2C komunikace pro senzor světla BH1750
  
  // Kontrola inicializace senzoru světla
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
    Serial.println("[BH1750] Failed to initialize the sensor!");
    while (1);
  }

  pinMode(lightPin, OUTPUT); // Nastavení pinu pro ovládání LED diody jako výstup

  setup_wifi();//Nastavení Wi-Fi připojení
  client.setServer(mqttServer, mqttPort);//Nastavení serveru a portu pro MQTT klienta
  client.setCallback(callback);

  delay(10);
}

void loop() {
  if (!client.connected()) {
    reconnect();//Pripojeni k MQTT
  }
  client.loop();//obsluha MQTT

  float lux = lightMeter.readLightLevel();//cteni hodnot ze senzoru
  printLux(lux);
  controlLED(lux);// Ovládání napětí LED diody na základě úrovně svitu

  char luxString[8];
  dtostrf(lux, 1, 2, luxString);//prevedeni svitu na retezec
  client.publish(mqttTopic, luxString);//odesilani hodnot svitu na mqtt

  delay(1000);
}