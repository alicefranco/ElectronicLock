#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include "MFRC522.h"
#include <ESP8266HTTPClient.h>
#include <aJSON.h>
#include <DS1307.h>


extern "C" {
#include "user_interface.h"
}

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/


//define pins
#define RST_PIN 5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5
#define SS_PIN  4

#define TRAVA 0
#define BUZZER 2
#define LED_WOFF 10
#define LED_WON 9
#define LED_PR 3
#define LED_PG 1
#define CLK1 16 //SDA
#define CLK2 15 //SCL


//auth parameters
const char *ssid = "Dermoestetica Visitantes";
const char *pass = "dermo2018";
const char *httpdestination = "http://www.appis.com.br/pontosys/api/ponto_funcionarios"; //http://192.168.15.26:8081
//const char *httpdestination-tags = "http://www.appis.com.br/pontosys/api/cartoes_rfid/alltags;" //http://192.168.15.26:8081

//card vars
int num_card, num_card_logs, logs_max;
const int num_max = 10;
String saved_cards[num_max];
String logs[num_max][2];

//status vars
String hora = "00:00:00";
String data = "01.01.18";
int attempts = 0;
bool start = false;

//bool conectado = false;
//int tentativas = 0;

//init
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
//DS1307 rtc(CLK1, CLK2); //SDA, SCL

//StaticJsonBuffer<1000> b;
//JsonObject* payload = &(b.createObject());

void mensagemConectado(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Conectado.");
  
  //lcd.clear();
  //lcd.setCursor(0,0);
  //lcd.print("Conectado.");
  delay(1000);
}

void mensagemNaoConectado(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Não conectado.");
  
  //lcd.clear();
  //lcd.setCursor(0,0);
  //lcd.print("Nao conectado.");
  //lcd.setCursor(0,1);
  //lcd.print("Conectando...");
  //delay(2000);
}

class Wifi
{
  public:
  Wifi(char *ssid,
    char *senha)
  {
    this->ssid = ssid;
    this->senha = senha;
    this->tentativas = 0;
    this->conectado = true;

    WiFi.begin(this->ssid, this->senha);
  }

  void conectar()
  {
    int status = WiFi.status();
    if (status != WL_CONNECTED) {
      if (this->conectado)
      {
        this->conectado = false;
        mensagemNaoConectado();
        WiFi.disconnect();
        WiFi.begin(this->ssid, this->senha);
      }
      
      
      if (this->tentativas > 1200)
      {
        mensagemNaoConectado();
        WiFi.disconnect();
        WiFi.begin(this->ssid, this->senha);
        this->tentativas = 0;
      }

      this->tentativas++;
    }
    else if (!conectado && WiFi.status() == WL_CONNECTED)
    {
      mensagemConectado();
      mensagemInicial();
      this->conectado = true;
    }
  }

  private:
    char *ssid;
    char *senha;
    int tentativas;
    bool conectado;
};

Wifi *wifi = new Wifi("Dermoestetica Visitantes", "dermo2018"); 
//Wifi *wifi = new Wifi("GVT-0FAD", "0073440455"); 
//Wifi *wifi = new Wifi("CONECTE", "conecte2018");





void setup() {
//  //Aciona o relogio
//  rtc.halt(false);
//  
//  //As linhas abaixo setam a data e hora do modulo
//  //e podem ser comentada apos a primeira utilizacao
//  //rtc.setDOW(THURSDAY);      //Define o dia da semana
//  //rtc.setTime(14, 37, 0);     //Define o horario
//  //rtc.setDate(16, 02, 2018);   //Define o dia, mes e ano
//  
//  //Definicoes do pino SQW/Out
//  rtc.setSQWRate(SQW_RATE_1);
//  rtc.enableSQW(true);
  
  //saved cards
  num_card = 0;
  num_card_logs = 0;
  
  //ledcSetup(channel, 2000, resolution);
  //ledcAttachPin(BUZZER, channel); 

  pinMode(LED_WON, OUTPUT);
  pinMode(LED_WOFF, OUTPUT);
//  pinMode(LED_PR, OUTPUT);
//  pinMode(LED_PG, OUTPUT);
//
//  ledConnection();
  
  pinMode(BUZZER, OUTPUT); //Initiate lock
  pinMode(TRAVA, OUTPUT);
  
  Serial.begin(9600);    // Initialize serial communications
  delay(250);
  
  //connection init
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  
  mensagemInicial();

  //HTTPConect();
}

void loop() {
   wifi->conectar();

   //ledConnection();

  // Look for new cards
  String rfid = readCard();
  Serial.println(rfid);
  //if connection is established
  if(rfid == "error"){
    mensagemTagProblem();
    mensagemInicial();
  }
  else if(WiFi.status() == WL_CONNECTED){
    newAccess(rfid);
  }
  //if not connected
  else if(rfid != "timeout" && WiFi.status() != WL_CONNECTED){
    logCard(rfid, false);
    mensagemInicial();
  }
}

void newAccess(String rfid){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  String message = createMsgUrlEnc(rfid, formatTime(hora, data));

  //connection to server
  int httpCode = sendPOST(httpdestination, message);
  Serial.println(httpCode);

  if(rfid != "timeout"){
    if(httpCode == 200){
      //String datainfo = (*payload)["data"];
      saveCard(rfid);
      digitalWrite(TRAVA, HIGH);
      mensagemEntradaLiberada();
      delay(1000);
      digitalWrite(TRAVA, LOW);
      mensagemPortaTravada();
      
      mensagemInicial();
    }
    else if(httpCode == 403){
      mensagemCartaoNaoAut();
      mensagemInicial();
    }
    //connection failed, try again
    else if(httpCode == -11){
      delay(1);
      logCard(rfid, true);
      mensagemInicial(); 
    }
    else if(httpCode == -1){
      logCard(rfid, false);
      mensagemInicial();
    }
  }
}
 
String readCard(){
  int attempts1 = 0;
  int attempts2 = 0;
  bool erro = false;
  while (attempts1 < 6000 && !mfrc522.PICC_IsNewCardPresent()) {
    wifi->conectar(); 
    //HTTPConect();
    debugIsOnTheTable();
    backupCards();
    delay(50);
    attempts1++; 
    //return;
  }
  
  // Select one of the cards
  while( attempts2 < 10 && !mfrc522.PICC_ReadCardSerial()) {
    wifi->conectar();
    delay(50);
    attempts2++;
    //return;
  }

  //return if an error reading the card ocurred
  if(attempts1 >= 6000){
    attempts1 = 0;
    return "timeout";
  }
  else if(attempts2 >= 10) 
  {
    attempts2 = 0;
    return "error";
  }
 
  
  //Show UID on serial monitor
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    //Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    //Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  String card = content.substring(1);
  return card;
}

void saveCard(String rfid){
  bool already_saved = false;
  for(int i = 0; i < num_max; i++){
    if(saved_cards[i] == rfid){  
      already_saved = true;
    }
  }    
  if(already_saved == false){
    //hora = rtc.getTimeStr();
    //data = rtc.getDateStr(FORMAT_SHORT);
    Serial.print(formatTime(hora, data));
    Serial.println(" Cartão salvo.");                           
    saved_cards[num_card] = rfid;
    if(num_card < num_max - 1){
        num_card++;   
    }
    else num_card = 0;
  }
  else already_saved = false;
  
  //debug
  for(int i = 0; i < num_max; i++){
    Serial.print("saved_cards[i]: ");
    Serial.println(saved_cards[i]);
  }   
}

void debugIsOnTheTable(){
  uint32_t free = system_get_free_heap_size();
  //hora = rtc.getTimeStr();
  //Serial.println(hora.substring(6,8));
  if(hora.substring(6,8) == "01"){
  
    //data = rtc.getDateStr(FORMAT_SHORT);
    
    Serial.print(formatTime(hora, data));
    Serial.print(" Free memory: ");
    Serial.println(free);
  
  }
  //wake watchdog
  delay(1);
  ESP.wdtFeed();
}

void logCard(String rfid, bool tryagain){
  bool stored = false;
  bool already_saved = false;
  //check if the card was already locally registered
  for(int i = 0; i < num_max; i++){
    //if saved, store it in the logs
    if(saved_cards[i] == rfid){
      logs[num_card_logs][0] = rfid;
      
      //hora = rtc.getTimeStr();
      //data = rtc.getDateStr(FORMAT_SHORT);
      Serial.print(formatTime(hora, data));
      Serial.print(" Cartão salvo no log de acesso: ");
      Serial.println(rfid);
      
      logs[num_card_logs][1] = formatTime(hora, data);
      digitalWrite(TRAVA, HIGH);
      mensagemEntradaLiberada();
      delay(1000);
      digitalWrite(TRAVA, LOW);
      mensagemPortaTravada();

      //check array limits
      if(num_card_logs < num_max - 1){
          num_card_logs++;  
      }
      else num_card_logs = 0;
      if(logs_max < num_max - 1){
          logs_max++;  
      }
      stored = true;
      break;
    }
  }
  
  Serial.println("\nLOGS DE ACESSO");
  for(int i = 0; i < logs_max; i++){
    Serial.print("logs[i][0]: ");
    Serial.println(logs[i][0]);
  }
  Serial.print("\n");
  
  //if not stored locally
  if(stored == false && tryagain){
    newAccess(rfid);
  }
  else if(stored == false && !tryagain){
    mensagemCartaoNaoAut();
    mensagemInicial();
  }
  stored = false;
}

void backupCards(){
  int num_ns = 0;
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  int numHora = (hora.substring(3, 5)).toInt();
  //if((hora.substring(0, 5) == "00:00")  && WiFi.status() == WL_CONNECTED){
  if((numHora > 00)  && WiFi.status() == WL_CONNECTED){
      
    while((logs_max > 0) || (num_ns > 0)){
      Serial.print(formatTime(hora, data));
      delay(1);
      for(int i = 0; i<logs_max; i++){
        delay(1);
        int httpCode;
        String rfid = logs[i][0];
        String datahora = logs[i][1];
        String message = createMsgUrlEnc(rfid, datahora);
        
        //connection to server
        httpCode = sendPOST(httpdestination, message);
        Serial.print(httpCode);
        if(httpCode == 200){
          Serial.println(" Backup OK.");
        }  
        else if(httpCode == 403){
          Serial.println(" Cartão não autorizado.");
        }
        else{
          Serial.println(" Backup falhou."); 
          logs[num_ns][0] = rfid;
          if(num_ns > num_max - 1) num_ns = 0;
          else num_ns++;
        }
      } 
      logs_max = num_ns;
      num_ns = 0;
    }
  }
}

int sendPOST(String httpdestination, String body){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient

      http.begin(httpdestination);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header


      httpCode = http.POST(body);   //Send the request
      String pl = http.getString();                  //Get the response payload
      //char pl2[1000];
      //pl.toCharArray(pl2, 1000);


      //JsonObject* x;
      //StaticJsonBuffer<1000> JSONBuffer;   //Memory pool
      //x = &(JSONBuffer.parseObject(pl2)); //Parse message

      //payload = x;

      Serial.println(httpCode);   //Print HTTP return code
      Serial.println(pl);    //Print request response payload


      http.end();  //Close connection

    }else{

       Serial.println("Error in WiFi connection");
    }
    return httpCode;
}

String createMsgUrlEnc(String rfid, String datehour){
  String form = "RFID=" + rfid;
  form = form + "&Databackup=" + datehour;
  return form;
}



//init msg
void mensagemInicial() {
  Serial.println(F("Aproxime seu cartão"));
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Aproxime seu");
//  lcd.setCursor(0, 1);
//  lcd.print("cartao do leitor");
}

void mensagemAcaoNegada(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  
  Serial.println(" Ação negada!");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Acao negada!");
  longBeep();
  delay(2000);
}


void mensagemTagProblem(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);                  
  Serial.print(formatTime(hora, data));
  Serial.println(" Erro na leitura do cartao.");
                     
//  lcd.clear();
//  lcd.setCursor(0,0);
//  lcd.print("Erro na leitura");
//  lcd.setCursor(0,1);
//  lcd.print("do cartao.");
  longBeep();
  delay(2000);
}

void mensagemEntradaLiberada(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
              
  Serial.println(" Entrada liberada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Entrada liberada");
  //ledConnection();
  digitalWrite(LED_PR, LOW);
  digitalWrite(LED_PG, HIGH);
  beep();
  delay(2000);
}

void mensagemPortaTravada(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  
  Serial.println(" Porta Travada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Porta Travada");
  //ledConnection();
  digitalWrite(LED_PR, HIGH);
  digitalWrite(LED_PG, LOW);
  beep();
  delay(2000);
}

void mensagemCartaoNaoAut(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  
  Serial.println(" Cartão não autorizado.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Cartão não");
//  lcd.setCursor(0, 1);
//  lcd.print("autorizado.");
  longBeep();
  delay(2000);
}

/*void tone(int freq, float sec){
  ledcWriteTone(channel, 2000);
  ledcWrite(channel, freq);
  delay(sec*1000);
  ledcWrite(channel, 0);
}

void beep(){
  tone(255, 0.1);
  delay(100);
  tone(255, 0.1);
}

void longBeep(){
  tone(255, 1);
}*/

void beep(){
  tone(BUZZER, 3000);
  delay(150);
  noTone(BUZZER);
  delay(100);
  tone(BUZZER, 3000);
  delay(150);
  noTone(BUZZER);
}

void longBeep(){
  tone(BUZZER, 3000);
  delay(1500);
  noTone(BUZZER);
}

/*void ledConnection(){
  // put your main code here, to run repeatedly:
  if(WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_WON, HIGH);
    digitalWrite(LED_WOFF, LOW);
    digitalWrite(LED_PR, HIGH);
    digitalWrite(LED_PG, LOW);
  }
  //delay(1000);
  else{
    digitalWrite(LED_WON, LOW);
    digitalWrite(LED_WOFF, HIGH);
    digitalWrite(LED_PR, HIGH);
    digitalWrite(LED_PG, LOW);
   // delay(1000);
  }
}*/

String formatTime(String hora, String data){
  String datahora = "20" + data.substring(6, 8) + "-" + data.substring(3,5) + "-" + data.substring(0,2)+ " " + hora;
  return datahora;
}

/*
void HTTPConect(){
  http.begin(httpdestination);
  http.setTimeout(50);
  http.setReuse(true);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header
  http.addHeader("Connection", "Keep-Alive");  //Specify content-type header
  http.addHeader("Cache-Control", "no-cache");
}
*/

    


