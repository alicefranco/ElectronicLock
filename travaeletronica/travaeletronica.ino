
#include <ArduinoJson.h>
#include <aJSON.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
//#include <LiquidCrystal_I2C.h>
#include <DS1307.h>

//define pins
#define RST_PIN  17  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5
#define SS_PIN  5
#define TRAVA 15
#define BUZZER 16
#define LED_WOFF 26
#define LED_WON 27
#define LED_PR 32
#define LED_PG 33
//#define CLK1 0 //SDA
//#define CLK2 2 //SCL

//extern "C" {
//#include "user_interface.h"
//}

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

int channel = 0;
int resolution = 8;

//bool conectado = false;
//int tentativas = 0;

//init
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
//LiquidCrystal_I2C lcd(0x3F,16,2);  //Create LCD instance
//DS1307 rtc(CLK1, CLK2); //SDA, SCL

StaticJsonBuffer<1000> b;
JsonObject* payload = &(b.createObject());

void setup() {
  //saved cards
  num_card = 0;
  num_card_logs = 0;
  
  ledcSetup(channel, 2000, resolution);
  ledcAttachPin(BUZZER, channel); 

  pinMode(LED_WON, OUTPUT);
  pinMode(LED_WOFF, OUTPUT);
  pinMode(LED_PR, OUTPUT);
  pinMode(LED_PG, OUTPUT);

  ledConnection();
  
  //Wire.begin(2,0);
  //lcd.begin();
  //lcd.clear();
  //lcd.noBacklight();

  //Aciona o relogio
  //rtc.halt(false);
  
  //As linhas abaixo setam a data e hora do modulo
  //e podem ser comentada apos a primeira utilizacao
  //rtc.setDOW(THURSDAY);      //Define o dia da semana
  //rtc.setTime(15, 20, 0);     //Define o horario
  //rtc.setDate(29, 01, 2018);   //Define o dia, mes e ano
  
  //Definicoes do pino SQW/Out
  //rtc.setSQWRate(SQW_RATE_1);
  //rtc.enableSQW(true);
  
  pinMode(BUZZER, OUTPUT); //Initiate lock
  pinMode(TRAVA, OUTPUT);
  
  Serial.begin(9600);    // Initialize serial communications
  delay(250);
  
  //connection init
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522

  WiFi.begin(ssid, pass);
  mensagemInicial();
}

void loop() {
  conectar();
  
  ledConnection();
  //getting real time
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);

  Serial.println(hora);
  Serial.println(data);
  
  // Look for new cards
  String rfid = readCard();

  //if connection is established
  if(rfid == ""){
    mensagemTagProblem();
    mensagemInicial();
  }
  else if(WiFi.status() == WL_CONNECTED){
    newAccess(rfid, false);
  }
  //if not connected
  else if(WiFi.status() != WL_CONNECTED){
    logCard(rfid);
    mensagemInicial();
  }
}

void newAccess(String rfid, bool secReq){
  Serial.println("r11");
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  String message = createMsgUrlEnc(rfid, formatTime(hora, data));
 
  //connection to server
  int httpCode = sendPOST(httpdestination, message);

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
  else if ((httpCode == -11 || httpCode == -1) && secReq == false) {
    logCard(rfid);
    mensagemInicial();
  }
  else if(secReq){
    mensagemCartaoNaoAut();						
    mensagemInicial();
	 
  }
}
 
String readCard(){
  int tentativas = 0;
  bool erro = false;
  while ( ! mfrc522.PICC_IsNewCardPresent()) {
    conectar(); 
    ledConnection();
    debugIsOnTheTable();
    backupCards();
    delay(50);
    //return;
  }
  
  // Select one of the cards
  while( tentativas < 40 && !mfrc522.PICC_ReadCardSerial()) {
    conectar();
    ledConnection();
    Serial.println("DEU MERDA!!!");
    delay(50);
    tentativas++;
    //return;
  }

  if (tentativas >= 40)
  {
    tentativas = 0;
    erro = true;
  }

  if (erro)
  {
    erro = false;
    return "";
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

  Serial.println("\nMessage : ");
  content.toUpperCase();

  String card = content.substring(1);
  Serial.println(card);
  
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
  if(hora.substring(6,8) == "00"){
    Serial.println("A20");
  
    //hora = rtc.getTimeStr();
    //data = rtc.getDateStr(FORMAT_SHORT);
    
    Serial.println("A21");
    Serial.print(formatTime(hora, data));

    Serial.println("A22");

    Serial.print(" ");
    Serial.print(free);
  
  }
  //wake watchdog
  yield();
  //ESP.wdtFeed();
}


void logCard(String rfid){
  Serial.println(rfid);
  bool stored = false;
  //check if the card was already locally registered
  Serial.println("aqui1");
  for(int i = 0; i < num_max; i++){
    Serial.println("aqui3");
    if(saved_cards[i] == rfid){    
      Serial.println("aqui5");
	  
      logs[num_card_logs][0] = rfid;
      
      //hora = rtc.getTimeStr();
      //data = rtc.getDateStr(FORMAT_SHORT);
      Serial.print(formatTime(hora, data));
      Serial.println(" Cartão salvo no log de acesso.");
      
      logs[num_card_logs][1] = formatTime(hora, data);
      												 
      
      digitalWrite(TRAVA, HIGH);
      mensagemEntradaLiberada();
      delay(7000);
      digitalWrite(TRAVA, LOW);
      mensagemPortaTravada();
      mensagemInicial();
      
      if(num_card_logs < num_max - 1){
          Serial.println("aqui6");
          num_card_logs++;  
      }
      else num_card_logs = 0;
      if(logs_max < num_max - 1){
          Serial.println("aqui7");
          logs_max++;  
      }
   
      Serial.println("aqui8");
      
     stored = true;
     break;
    }
  }
  for(int i = 0; i < logs_max; i++){
    Serial.print("logs[i][0]");
    Serial.println(logs[i][0]);
  }
  if(stored == false){
	  newAccess(rfid, true);					   
  }
  stored = false;
}

void backupCards(){
  int num_ns = 0;
  if((hora.substring(0, 5) == "00:00")  && WiFi.status() == WL_CONNECTED){
    Serial.println("back1");
	  //hora = rtc.getTimeStr();
    //data = rtc.getDateStr(FORMAT_SHORT);
    Serial.print(formatTime(hora, data));
    									
    while((logs_max > 0) || (num_ns > 0)){
      Serial.print("nums: ");
      Serial.println(num_card_logs);
      Serial.println(num_ns);
      Serial.println(logs_max);
      for(int i = 0; i<logs_max; i++){
        Serial.println("back4");
        int httpCode;
        String rfid = logs[i][0];
        String datahora = logs[i][1];
        String message = createMsgUrlEnc(rfid, datahora);
        Serial.println("back5");
        //connection to server
        httpCode = sendPOST(httpdestination, message);
        Serial.println("back6");
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
        Serial.println("back7");
      } 
      Serial.println("back8");
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

String createMsgUrlEnc(String rfid, String datehour){
  String form = "RFID=" + rfid;
  form = form + "&Databackup=" + datehour;
  return form;
}

String formatTime(String hora, String data){
  String datahora = "20" + data.substring(6, 8) + "-" + data.substring(3,5) + "-" + data.substring(0,2)+ " " + hora;
  return datahora;
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
  Serial.println(" Erro na leitura do cartao.");
  
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
  Serial.println(" Erro na leitura do cartao.");
  					  
  Serial.println(" Entrada liberada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Entrada liberada");
  ledConnection();
  digitalWrite(LED_PR, LOW);
  digitalWrite(LED_PG, HIGH);
  beep();
  delay(2000);
}

void mensagemPortaTravada(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Erro na leitura do cartao.");
  
  Serial.println(" Porta Travada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Porta Travada");
  ledConnection();
  digitalWrite(LED_PR, HIGH);
  digitalWrite(LED_PG, LOW);
  beep();
  delay(2000);
}

void mensagemCartaoNaoAut(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Erro na leitura do cartao.");
  
  Serial.println(" Cartão não autorizado.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Cartão não");
//  lcd.setCursor(0, 1);
//  lcd.print("autorizado.");
  longBeep();
  delay(2000);
}

void mensagemConectado(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Erro na leitura do cartao.");
  
  Serial.println(F(" Conectado."));
//  lcd.clear();
//  lcd.setCursor(0,0);
//  lcd.print("Conectado.");
  delay(1000);
}

void mensagemNaoConectado(){
  //hora = rtc.getTimeStr();
  //data = rtc.getDateStr(FORMAT_SHORT);
  Serial.print(formatTime(hora, data));
  Serial.println(" Erro na leitura do cartao.");
  
  Serial.println(F(" Nao conectado."));
  //lcd.clear();
  //lcd.setCursor(0,0);
  //lcd.print("Nao conectado.");
  //lcd.setCursor(0,1);
  //lcd.print("Conectando...");
  //delay(2000);
}

void tone(int freq, float sec){
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
}

void ledConnection(){
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
}

void conectar()
{
  /*
  int status = WiFi.status();
  if (status != WL_CONNECTED) {
    if (conectado)
    {
      conectado = false;
      mensagemNaoConectado();
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
    }
    
    
    if (tentativas > 1200)
    {
      mensagemNaoConectado();
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
      tentativas = 0;
    }

    tentativas++;
  }
  else if (!conectado && WiFi.status() == WL_CONNECTED)
  {
    mensagemConectado();
    mensagemInicial();
    conectado = true;
  }*/
  if(WiFi.status() == WL_CONNECTED){
    if(attempts != 0){
      Serial.println("CON");
      attempts = 0;
    }
  }
  else if (attempts == 0) {
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    Serial.println("NOT");
    delay(1000);
    attempts++;
  }
  else if(WiFi.status() != WL_CONNECTED){
    delay(1000);
    if(attempts < 10){
      attempts++;
    }
    else attempts = 0;
  }
  delay(1000);
}


    


