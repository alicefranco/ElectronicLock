/*
Many thanks to nikxha from the ESP8266 forum
*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include "MFRC522.h"
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
const char *httpdestination = "http://10.0.0.126:80/clinicaapi/api/registro_acessos";//"http://httpbin.org/post"; //server
//String sala = "001A"; //room where the lock is placed


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance

int tr_dest = 1;
int connected = 0;

int num_card;
String saved_cards[20];

void setup() {
  //saved cards
  num_card = 3;
  saved_cards[0] = "70 F1 E0 2B";
  saved_cards[1] = "F9 EC DE 2B";


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
    lcd.print("Conectado.");
    delay(3000);
    connected = 1;
  }
  if(connected == 1){
    Serial.println(F("======================================================"));
    Serial.println(F("Pronto para ler cartão UID: \n"));
    mensagemInicial(); //Print init message
    delay(3000);
  }
  else{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Nao conectado.");
    lcd.setCursor(0,1);
    lcd.print("Tente novamente.");
    delay(3000);
    connected = 0;
  }
}

void loop() {

  // Look for new cards
  while ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  while( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  //Show UID on serial monitor
  Serial.print(F("UID tag : "));
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.println(F("Message : "));
  content.toUpperCase();


  String card = content.substring(1);
  Serial.println("card");
  Serial.println(card);

  char aux1[11] ;
  char aux2[11] ;

  saved_cards[0].toCharArray(aux1, 15);
  card.toCharArray(aux2, 15);

  //unlock if any of the cards saved were used
  int auth = 0; //acesso autorizado
  for(int i = 0; i < num_card; i++){
    if (saved_cards[i] == card )//compara cartões salvos com cartão lido
    {
      auth = 1;
    }
  }

  //authorized UIDs
  if (auth == 1)
  {
    int httpCode;
    bool stat;
    String message = "message";
    String uid = card;
    //Locked door, unlock it
    if (tr_dest == 1) {
      stat = true;
      message = createMsgJSON(uid, stat);
      httpCode = sendPOST(message);
      if(httpCode == 201){
        tr_dest = 0; //door unlocked
        digitalWrite(TRAVA, HIGH);
        mensagemEntradaLiberada();
        delay(3000);
        mensagemInicial();
      }
    }

    //Unlocked door, lock it.
    else {
      stat = false;
      message = createMsgJSON(uid, stat);
      httpCode = sendPOST(message);
      if(httpCode == 201){
        tr_dest = 1; //door locked
        digitalWrite(TRAVA, LOW);
        mensagemPortaTravada();
        delay(3000);
        mensagemInicial();
      }
    }
  }

  //not an authorized UID
  else {
    mensagemAcaoNegada();
    delay(3000);
    mensagemInicial();
  }
}

// Helper routine to dump a byte array as hex values to Serial
/*void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}*/

//init msg
void mensagemInicial() {
  Serial.println(F("Aproxime seu cartão"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aproxime seu");
  lcd.setCursor(0, 1);
  lcd.print("cartao do leitor");

}

//send to server
int sendPOST(String message){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient

      http.begin(httpdestination);
      http.addHeader("Content-Type", "application/json");  //Specify content-type header

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
String createMsgJSON(String uid, bool stat){
  aJsonObject* root = aJson.createObject();
  //aJson.addStringToObject(root, "nome", nome.c_str());
  aJson.addStringToObject(root, "RFID", uid.c_str());
  aJson.addBooleanToObject(root, "Status", stat);
  aJson.addNumberToObject(root, "Id_Local_Acesso", 2);
  String json_object = aJson.print(root);
  Serial.println(aJson.print(root));
  return json_object;
}

void mensagemEntradaLiberada(){
  Serial.println("Entrada liberada.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ola!");
  lcd.setCursor(0, 1);
  lcd.print("Entrada liberada");
  delay(1000);
}

void mensagemPortaTravada(){
  Serial.println("Porta travada.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Porta travada.");
  delay(1000);
}

void mensagemAcaoNegada(){
  Serial.println(F("Ação negada!"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Acao negada!");
  delay(1000);
}
