/*
Many thanks to nikxha from the ESP8266 forum
*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include "MFRC522.h"
#include <LiquidCrystal.h>
#include <ESP8266HTTPClient.h>
#include <aJSON.h>
#include <LiquidCrystal_I2C.h>

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4 
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/

#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  4  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4 
#define TRAVA 15

const char *ssid =  "Dermo-WiFi";     // change according to your Network - cannot be longer than 32 characters!
const char *pass =  "dermo7560"; // change according to your Network
const char *httpdestination = "http://httpbin.org/post"; //server
String sala = "001A"; //room where the lock is placed


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance 

int tr_dest = 1;

void setup() {
  Wire.begin(2,0);
  lcd.init();   
  lcd.noBacklight();

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked
 
  Serial.begin(9600);    // Initialize serial communications
  delay(250);
  Serial.println(F("Booting...."));
  
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  
  WiFi.begin(ssid, pass); // Initialize wifi connection
  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 100)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
   
    Serial.println(F("WiFi conectado."));
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connected.");
    delay(3000);
  }

  Serial.println(F("======================================================")); 
  Serial.println(F("Pronto para ler cartão UID: \n"));
  mensagemInicial(); //Print init message
  delay(3000);
}

void loop() { 

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  //Show UID on serial monitor
  Serial.print(F("UID tag : "));
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print(F("Message : "));
  content.toUpperCase();

  //authorized UIDs 
  if (content.substring(1) == "70 F1 E0 2B") 
  {
    int httpCode;
    String nome = "Emerson";
    String stat;
    String message = "message";
    String uid = "70 F1 E0 2B";
    //Locked door, unlock it
    if (tr_dest == 1) {
      httpCode = sendPOST(message);
      stat = "true";
      message = createMsgJSON(nome, uid, stat);
      if(httpCode == 200){
        Serial.println(F("Acesso autorizado."));
        Serial.println();
        tr_dest = 0; //door unlocked
        digitalWrite(TRAVA, HIGH);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hello, Alice!");
        lcd.setCursor(0, 1);
        lcd.print("Door unlocked.");
        delay(3000);
        mensagemInicial();
      }
    }

    //Unlocked door, lock it.
    else {
      stat = "false";
      message = createMsgJSON(nome, uid, stat);
      httpCode = sendPOST(message); 
      if(httpCode == 200){
        Serial.println(F("Porta travada."));
        Serial.println();
        tr_dest = 1; //door locked
        digitalWrite(TRAVA, LOW);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Door locked.");
        lcd.setCursor(0, 1);
        lcd.print("");
        delay(3000);
        mensagemInicial();
      }
    }
  }

  //not an authorized UID
  else {
    Serial.println(F("Ação negada!"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Action denied!");
    delay(3000);
    mensagemInicial();
  }
}

// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//init msg
void mensagemInicial() {
  Serial.println(F("Aproxime seu cartão"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    Ready to    ");
  lcd.setCursor(0, 1);
  lcd.print("   read card:   ");

}

//send to server
int sendPOST(String message){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient
     
      http.begin(httpdestination);      
      http.addHeader("Content-Type", "text/plain");  //Specify content-type header
     
      httpCode = http.POST(message);   //Send the request
      String payload = http.getString();                  //Get the response payload
     
      Serial.println(httpCode);   //Print HTTP return code
      Serial.println(payload);    //Print request response payload
     
      http.end();  //Close connection
       
    }else{
     
       Serial.println("Error in WiFi connection");   
    }
    return httpCode;
}

//method to create the Json formatted msg
String createMsgJSON(String nome, String uid, String stat){
  aJsonObject* root = aJson.createObject();
  aJson.addStringToObject(root, "nome", nome.c_str());
  aJson.addStringToObject(root, "uid", uid.c_str());
  aJson.addStringToObject(root, "sala", sala.c_str());
  aJson.addStringToObject(root, "status", stat.c_str());
  String json_object = aJson.print(root);
  Serial.println(aJson.print(root));
  return json_object;
}

