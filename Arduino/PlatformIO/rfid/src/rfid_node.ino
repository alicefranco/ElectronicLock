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
const char *httpdestination = "http://192.168.15.101:8081/token";//"http://httpbin.org/post"; //server
//String sala = "001A"; //room where the lock is placed


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance

int tr_dest = 1;
int connected = 0;

int num_card;
String saved_cards[20];

String grant_type = "password";
String UserName = "sala2";
String password = "@Sala2";

aJsonObject* payload = aJson.createObject();

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

  int httpCode;

  bool stat = false;
  String rfid = card;
  String messageAuth = createForm();
  httpCode = sendPOST("", messageAuth, false);


 if(httpCode == 200){
    String message = createMsgJSON(rfid, stat);
    aJsonObject* token_ptr = aJson.createObject();
    token_ptr = aJson.getObjectItem(payload, "access_token");
    /*aJsonObject* token_type_ptr = aJson.getObjectItem(payload, "token_type");
    String token = token_ptr->valuestring;
    String token_type = token_type_ptr->valuestring;
    String header = token_type + " " + token;
    stat = !stat;
    httpCode = sendPOST(header , message, true);
    if(httpCode == 200){
      saved_cards[num_card] = card;
      num_card++;
      //Locked door, unlock it
      if (stat == true) {
        tr_dest = 0; //door unlocked
        digitalWrite(TRAVA, HIGH);
        mensagemEntradaLiberada();
        delay(3000);
        mensagemInicial();
      }
      //Unlocked door, lock it.
      else {
        tr_dest = 1; //door locked
        digitalWrite(TRAVA, LOW);
        mensagemPortaTravada();
        delay(3000);
        mensagemInicial();
      }
    }
    else{
      stat = !stat;
      mensagemAcaoNegada();
      delay(3000);
      mensagemInicial();
    }*/
  }
  else{
    stat = !stat;
    mensagemAcaoNegada();
    delay(3000);
    mensagemInicial();
  }
}
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
int sendPOST(String header, String body, bool auth){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient

      http.begin(httpdestination);
      if(auth) http.addHeader("Authorization", header);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header


      httpCode = http.POST(body);   //Send the request
      String pl = http.getString();                  //Get the response payload
      char pl2[1000];
      pl.toCharArray(pl2, 1000);
      payload = aJson.parse(pl2);


      Serial.println(httpCode);   //Print HTTP return code
      Serial.println(pl);    //Print request response payload


      http.end();  //Close connection

    }else{

       Serial.println("Error in WiFi connection");
    }
    return httpCode;
}

String createForm(){
  String form = "grant_type=" + grant_type + "&"
    + "UserName=" + UserName + "&"
    + "password=" + password;
  return form;
}




String createAuthJSON(){
  aJsonObject* root = aJson.createObject();
  //aJson.addStringToObject(root, "nome", nome.c_str());
  aJson.addStringToObject(root, "grant_type", grant_type.c_str());
  aJson.addStringToObject(root, "UserName", UserName.c_str());
  aJson.addStringToObject(root, "password", password.c_str());
  String json_object = aJson.print(root);
  Serial.println(aJson.print(root));
  return json_object;
}

//method to create the Json formatted msg
String createMsgJSON(String rfid, bool stat){
  aJsonObject* root = aJson.createObject();
  //aJson.addStringToObject(root, "nome", nome.c_str());
  aJson.addStringToObject(root, "RFID", rfid.c_str());
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
