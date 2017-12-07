/*
Many thanks to nikxha from the ESP8266 forum
*/
#include <Arduino.h>
#include <ArduinoJson.h>
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

const char *ssid =  "Dermoestetica";     // change according to your Network - cannot be longer than 32 characters!
const char *pass =  "dermoaju2017se"; // change according to your Network
const char *httpdestinationauth = "http://clinicaapi.gear.host/token";// "http://httpbin.org/post"; // //
const char *httpdestination = "http://clinicaapi.gear.host/api/cartoes_RFID/verifyrfid";


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance

int tr_dest = 1;
int connected = 0;

int num_card;
String saved_cards[20];

String grant_type = "password";
String UserName = "sala2";
String password = "@Sala2";

String ID_Local_Acesso = "2";
String stat = "false";

StaticJsonBuffer<1000> b;
JsonObject* payload = &(b.createObject());

void setup() {
  //saved cards
  num_card = 0;


  Wire.begin(2,0);
  lcd.init();
  lcd.backlight();

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked


  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, pass); // Initialize wifi connection
  }

  Serial.begin(9600);    // Initialize serial communications
  delay(250);
  mensagemConectando();

  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522


  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 100)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    mensagemConectado();
    connected = 1;
  }

  //if connected, gets read to read cards
  if(connected == 1){
    Serial.println(F("======================================================"));
    Serial.println(F("Pronto para ler cartão UID: \n"));
    mensagemInicial(); //Print init message
    delay(3000);
  }
  else{
    mensagemNaoConectado();
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


  String rfid = card;
  String messageAuth = createForm();
  httpCode = sendPOST(httpdestinationauth, "", messageAuth, false);


 if(httpCode == 200){
    String message = createMsgUrlEnc(rfid, stat);
    String access_token = (*payload)["access_token"];
    String token_type = (*payload)["token_type"];
    //aJsonObject* token_ptr = aJson.getObjectItem(payload, "access_token");
    //aJsonObject* token_type_ptr = aJson.getObjectItem(payload, "token_type");
    //String token = token_ptr->valuestring;
    //String token_type = token_type_ptr->valuestring;
    String header = token_type + " " + access_token;
    if(stat == "true"){
      stat = "false";
    }
    else if(stat == "false"){
      stat = "true";
    }


    httpCode = sendPOST(httpdestination, header, message, true);
    if(httpCode == 200){
      saved_cards[num_card] = card;
      num_card++;
      //Locked door, unlock it
      if (stat == "true") {
        tr_dest = 0; //door unlocked
        digitalWrite(TRAVA, HIGH);
        mensagemEntradaLiberada();
        delay(3000);
        mensagemInicial();
      }
      //Unlocked door, lock it.
      else if(stat == "false") {
        tr_dest = 1; //door locked
        digitalWrite(TRAVA, LOW);
        mensagemPortaTravada();
        delay(3000);
        mensagemInicial();
      }
    }
    else if(httpCode == 403){
      if(stat == "true") stat = "false";
      else if(stat == "false") stat = "true";
      mensagemCartaoNaoAut();
      delay(3000);
      mensagemInicial();
    }
    else{
      if(stat == "true") stat = "false";
      else if(stat == "false") stat = "true";
      mensagemAcaoNegada();
      delay(3000);
      mensagemInicial();
    }//*/
  }
  else{
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
int sendPOST(String httpdestination, String header, String body, bool auth){
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


      JsonObject* x;
      StaticJsonBuffer<1000> JSONBuffer;   //Memory pool
      x = &(JSONBuffer.parseObject(pl2)); //Parse message

      payload = x;

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

String createMsgUrlEnc(String rfid, String stat){
  String form = "RFID=" + rfid + "&"
    + "Status=" + stat + "&"
    +"ID_Local_Acesso=" + ID_Local_Acesso;
  return form;
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

void mensagemCartaoNaoAut(){
  Serial.println(F("Cartão não autorizado!"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cartao nao");
  lcd.setCursor(0, 1);
  lcd.print("autorizado.");
  delay(1000);
}

void mensagemConectado(){
  Serial.println(F("WiFi conectado."));
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Conectado.");
  delay(3000);
}

void mensagemNaoConectado(){
  Serial.println(F("Não conectado, tente novamente."));
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Nao conectado.");
  lcd.setCursor(0,1);
  lcd.print("Tente novamente.");
  delay(3000);
}

void mensagemConectando(){
  Serial.println(F("Conectando...."));
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Conectando...");
}
