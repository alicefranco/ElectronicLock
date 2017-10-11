# 1 "c:\\users\\market~1\\appdata\\local\\temp\\tmpjyjibq"
#include <Arduino.h>
# 1 "C:/Users/Marketing/Desktop/Repositorios/travaeletronica/Arduino/PlatformIO/rfid/src/pontoeletronico.ino"






#include <Arduino.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>

#include <SPI.h>

#include "MFRC522.h"

#include <ESP8266HTTPClient.h>

#include <aJSON.h>

#include <LiquidCrystal_I2C.h>
# 45 "C:/Users/Marketing/Desktop/Repositorios/travaeletronica/Arduino/PlatformIO/rfid/src/pontoeletronico.ino"
#define RST_PIN 5

#define SS_PIN 4

#define TRAVA 15



const char *ssid = "Dermo-WiFi";

const char *pass = "dermo7560";

const char *httpdestinationauth = "http://192.168.15.134:8081/token";

const char *httpdestination = "http://192.168.15.134:8081/api/cartoes_RFID/verifyrfid";
# 69 "C:/Users/Marketing/Desktop/Repositorios/travaeletronica/Arduino/PlatformIO/rfid/src/pontoeletronico.ino"
MFRC522 mfrc522(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x3F,16,2);



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
void setup();
void loop();
void mensagemInicial();
int sendPOST(String httpdestination, String header, String body, bool auth);
String createForm();
String createMsgUrlEnc(String rfid, String stat);
void mensagemEntradaLiberada();
void mensagemPortaTravada();
void mensagemAcaoNegada();
void mensagemCartaoNaoAut();
#line 107 "C:/Users/Marketing/Desktop/Repositorios/travaeletronica/Arduino/PlatformIO/rfid/src/pontoeletronico.ino"
void setup() {



  num_card = 3;

  saved_cards[0] = "70 F1 E0 2B";

  saved_cards[1] = "F9 EC DE 2B";





  Wire.begin(2,0);

  lcd.init();

  lcd.noBacklight();



  pinMode(TRAVA, OUTPUT);

  digitalWrite(TRAVA, LOW);

  tr_dest = 1;



  Serial.begin(9600);

  delay(250);

  Serial.println(F("Conectando...."));



  SPI.begin();

  mfrc522.PCD_Init();



  WiFi.begin(ssid, pass);

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

    mensagemInicial();

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





  while ( ! mfrc522.PICC_IsNewCardPresent()) {

    delay(50);

    return;

  }



  while( ! mfrc522.PICC_ReadCardSerial()) {

    delay(50);

    return;

  }



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
# 323 "C:/Users/Marketing/Desktop/Repositorios/travaeletronica/Arduino/PlatformIO/rfid/src/pontoeletronico.ino"
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



      if (stat == "true") {

        tr_dest = 0;

        digitalWrite(TRAVA, HIGH);

        mensagemEntradaLiberada();

        delay(3000);

        mensagemInicial();

      }



      else if(stat == "false") {

        tr_dest = 1;

        digitalWrite(TRAVA, LOW);

        mensagemPortaTravada();

        delay(3000);

        mensagemInicial();

      }

    }

    else{

      if(stat == "true") stat = "false";

      else if(stat == "false") stat = "true";

      mensagemAcaoNegada();

      delay(3000);

      mensagemInicial();

    }

  }

  else if(httpCode = 403){

    mensagemCartaoNaoAut();

    delay(3000);

    mensagemInicial();

  }

  else{

    mensagemAcaoNegada();

    delay(3000);

    mensagemInicial();

  }

}







void mensagemInicial() {

  Serial.println(F("Aproxime seu cartão"));

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("Aproxime seu");

  lcd.setCursor(0, 1);

  lcd.print("cartao do leitor");



}





int sendPOST(String httpdestination, String header, String body, bool auth){

  int httpCode;

  if(WiFi.status()== WL_CONNECTED){



      HTTPClient http;



      http.begin(httpdestination);

      if(auth) http.addHeader("Authorization", header);

      http.addHeader("Content-Type", "application/x-www-form-urlencoded");





      httpCode = http.POST(body);

      String pl = http.getString();

      char pl2[1000];

      pl.toCharArray(pl2, 1000);





      JsonObject* x;

      StaticJsonBuffer<1000> JSONBuffer;

      x = &(JSONBuffer.parseObject(pl2));



      payload = x;



      Serial.println(httpCode);

      Serial.println(pl);





      http.end();



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