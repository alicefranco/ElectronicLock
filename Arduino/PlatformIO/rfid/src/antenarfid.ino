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
#include <SoftwareSerial.h>


//#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5
//#define SS_PIN  4  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4
#define TRAVA 15

const char *ssid =  "Dermoestetica";     // change according to your Network - cannot be longer than 32 characters!
const char *pass =  "dermoaju2017se"; // change according to your Network
const char *httpdestinationauth = "http://clinicaapi.gear.host/token";// "http://httpbin.org/post"; // //
const char *httpdestination = "http://clinicaapi.gear.host/api/cartoes_RFID/verifyrfid";


unsigned char aux[14];
unsigned char auxf[1];
int long_tag;
unsigned char next;
unsigned char card[15];

SoftwareSerial serialArtificial(13, 14, false, 256); //1º TX do leitor, 2º RX do leitor
//LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance

int tr_dest = 1;
int connected = 0;

int num_card;
String saved_cards[20];
long time1, time2;


String grant_type = "password";
String UserName = "sala2";
String password = "@Sala2";

String ID_Local_Acesso = "1";
String stat = "false";

StaticJsonBuffer<1000> b;
JsonObject* payload = &(b.createObject());

void setup() {
  num_card = 0;

  long_tag = 0;

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked

  Serial.begin(9600);    // Initialize serial communications
  serialArtificial.begin(9600);

  delay(250);
  Serial.println(F("Conectando...."));


  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, pass); // Initialize wifi connection
  }
  
  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 100)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {

    Serial.println(F("WiFi conectado."));
    delay(1000);
    connected = 1;
  }

  //if connection is established, gets ready to ready tag
  if(connected == 1){
    Serial.println(F("======================================================"));
    Serial.println(F("Pronto para ler tag: \n"));
    mensagemInicial(); //Print init message
    delay(1000);
  }
  else{
    delay(1000);
    connected = 0;
  }

  //time setup
  time1 = millis();
  time2 = millis();
}

void loop() {
  if((millis() - time1) >= 10000){
    if(serialArtificial.available() > 0 ){
      time1 = millis();
      serialArtificial.readBytes(aux, 14);
      delay(250);

      next = serialArtificial.peek();
      if(next == aux[0]){
        long_tag = 0;
      }
      else if(next == 0xFF){
        long_tag = 0;
      }
      else{
        long_tag = 1;
        delay(250);
        serialArtificial.readBytes(auxf, 1);
      }

      for(int i = 0; i < 14; i++){
        card[i] = aux[i];
      }
      if(long_tag == 1){
        card[14] = auxf[0];
        long_tag = 0;
      }
      else{
        card[14] = 0;
      }
      serialArtificial.flush();
      delay(1000);

      int httpCode;
      String rfid;

      for(int i = 0; i<15; i++){
        rfid += String(card[i], HEX);
      }
      Serial.println(rfid);
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
          //saved_cards[num_card] = card;
          //num_card++;
          //Locked door, unlock it
          if (stat == "true") {
            tr_dest = 0; //door unlocked
            digitalWrite(TRAVA, HIGH);
            mensagemEntradaLiberada();
            mensagemInicial();
          }
          //Unlocked door, lock it.
          else if(stat == "false") {
            tr_dest = 1; //door locked
            digitalWrite(TRAVA, LOW);
            mensagemPortaTravada();
            mensagemInicial();
          }
        }
        else if(httpCode == 403){
          if(stat == "true") stat = "false";
          else if(stat == "false") stat = "true";
          mensagemCartaoNaoAut();
          mensagemInicial();
        }
        else{
          if(stat == "true") stat = "false";
          else if(stat == "false") stat = "true";
          mensagemAcaoNegada();
          mensagemInicial();
        }
      }
    }
    //Serial.println(time1);
  }
  else serialArtificial.flush();
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

//init msg
void mensagemInicial() {
  Serial.println(F("Aproxime seu cartão"));
}

void mensagemEntradaLiberada(){
  Serial.println("Entrada liberada.");
  delay(1000);
}

void mensagemPortaTravada(){
  Serial.println("Porta travada.");
  delay(1000);
}

void mensagemAcaoNegada(){
  Serial.println(F("Ação negada!"));
  delay(1000);
}

void mensagemCartaoNaoAut(){
  Serial.println(F("Cartão não autorizado!"));
  delay(1000);
}
