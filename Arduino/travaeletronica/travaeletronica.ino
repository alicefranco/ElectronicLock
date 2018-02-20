#include <time.h>
#include <ArduinoJson.h>
#include <aJSON.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>

//define pins
#define RST_PIN  17  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5
#define SS_PIN  5
#define TRAVA 15
#define BUZZER 16
#define LED_WOFF 26
#define LED_WON 27
#define LED_PR 32
#define LED_PG 33

//auth parameters
const char *ssid = "Dermoestetica Visitantes";
const char *pass = "dermo2018";
const char *httpdestination = "http://www.appis.com.br/pontosys/api/ponto_funcionarios"; //http://192.168.15.26:8081
//const char *httpdestination-tags = "http://www.appis.com.br/pontosys/api/cartoes_rfid/alltags;" //http://192.168.15.26:8081

//clock parameters
const char* NTP_SERVER = "br.pool.ntp.org";
//const char* TZ_INFO    = "EST5EDT4,M3.2.0/02:00:00,M11.1.0/02:00:00";
const char* TZ_INFO    = "UTC3,M3.2.0/02:00:00,M11.1.0/02:00:00";

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

//init
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

struct tm timeinfo;

//StaticJsonBuffer<1000> b;
//JsonObject* payload = &(b.createObject());

void setup() {
  //saved cards
  num_card = 0;
  num_card_logs = 0;
  
  ledcSetup(channel, 2000, resolution);
  ledcAttachPin(BUZZER, channel); 

  //init local clock
  configTzTime(TZ_INFO, NTP_SERVER);
  // start wifi here
  if (getLocalTime(&timeinfo, 10000)) {  // wait up to 10sec to sync
    Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
  } else {
    Serial.println("Time not set");
  }

  
  pinMode(BUZZER, OUTPUT); //Initiate lock
  pinMode(TRAVA, OUTPUT);
  pinMode(LED_WON, OUTPUT);
  pinMode(LED_WOFF, OUTPUT);
  pinMode(LED_PR, OUTPUT);
  pinMode(LED_PG, OUTPUT);

  digitalWrite(LED_PR, HIGH);
  
  Serial.begin(9600);    // Initialize serial communications
  delay(250);
  
  //connection init
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522

  WiFi.begin(ssid, pass);
  mensagemInicial();
  
  //HTTPConect();
}

void loop() {
  conectar();

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
  String message = createMsgUrlEnc(rfid, getLTime());

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
    conectar(); 

    //sync time with NTP server
    getRTime();
    //HTTPConect();
    debugIsOnTheTable();
    //backup locally saved cards
    backupCards();
    delay(50);
    attempts1++; 
    //return;
  }
  
  // Select one of the cards
  while(attempts2 < 10 && !mfrc522.PICC_ReadCardSerial()) {
    conectar();
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
    Serial.print(getLTime());
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
  hora = getLHour();
  data = getLDate();
  if(hora.substring(6,8) == "00"){
    Serial.print(getLTime());
    Serial.print(" Free memory: ");
    Serial.println(free);
  
  }
  //wake watchdog
  delay(1);
  //ESP.wdtFeed();
}

void logCard(String rfid, bool tryagain){
  bool stored = false;
  bool already_saved = false;
  //check if the card was already locally registered
  for(int i = 0; i < num_max; i++){
    //if saved, store it in the logs
    if(saved_cards[i] == rfid){
      logs[num_card_logs][0] = rfid;
    
      Serial.print(getLTime());
      Serial.print(" Cartão salvo no log de acesso: ");
      Serial.println(rfid);
      
      logs[num_card_logs][1] = getLTime();
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
  if((hora.substring(0, 5) == "00:00")  && WiFi.status() == WL_CONNECTED){
    Serial.print(getLTime());
    									
    while((logs_max > 0) || (num_ns > 0)){
      for(int i = 0; i<logs_max; i++){
        int httpCode;
        String rfid = logs[i][0];
        String datahora = logs[i][1];
        String message = createMsgUrlEnc(rfid, datahora);
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

/*String formatTime(String hora, String data){
  String datahora = "20" + data.substring(6, 8) + "-" + data.substring(3,5) + "-" + data.substring(0,2)+ " " + hora;
  return datahora;
}*/

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
  Serial.print(getLTime());
  
  Serial.println(" Ação negada!");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Acao negada!");
  longBeep();
  delay(2000);
}


void mensagemTagProblem(){							  
	Serial.print(getLTime());
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
  Serial.print(getLTime());
  					  
  Serial.println(" Entrada liberada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Entrada liberada");
  digitalWrite(LED_PR, LOW);
  digitalWrite(LED_PG, HIGH);
  beep();
  delay(2000);
}

void mensagemPortaTravada(){
  Serial.print(getLTime());
  
  Serial.println(" Porta Travada.");
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Ola!");
//  lcd.setCursor(0, 1);
//  lcd.print("Porta Travada");
  digitalWrite(LED_PR, HIGH);
  digitalWrite(LED_PG, LOW);
  beep();
  delay(2000);
}

void mensagemCartaoNaoAut(){
  Serial.print(getLTime());
  
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
  Serial.print(getLTime());
  
  Serial.println(F(" Conectado."));
//  lcd.clear();
//  lcd.setCursor(0,0);
//  lcd.print("Conectado.");
  delay(1000);
}

void mensagemNaoConectado(){
  Serial.print(getLTime());
  
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


void conectar()
{
 
  if(WiFi.status() == WL_CONNECTED){
    if(attempts != 0){
      mensagemConectado();
      digitalWrite(LED_WON, HIGH);
      digitalWrite(LED_WOFF, LOW);
      attempts = 0;
    }
  }
  else if (attempts == 0) {
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    mensagemNaoConectado();
    digitalWrite(LED_WON, LOW);
    digitalWrite(LED_WOFF, HIGH);
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

void getRTime(){
  if(WiFi.status() == WL_CONNECTED && hora.substring(0,1) == "00"){
     configTzTime(TZ_INFO, NTP_SERVER);
  } 
}

String getLHour(){
  getLocalTime(&timeinfo);
  String hr, mn, sc;
  String localh;
  if(timeinfo.tm_hour < 10) hr = "0" + String(timeinfo.tm_hour);
  else hr = String(timeinfo.tm_hour);
  
  if(timeinfo.tm_min < 10) mn = "0" + String(timeinfo.tm_min); 
  else mn = String(timeinfo.tm_min); 

  if(timeinfo.tm_sec < 10) sc = "0" + String(timeinfo.tm_sec);
  else sc = String(timeinfo.tm_sec);
 
  
  localh = hr +":"+ mn +":"+ sc;
  return localh;  
}

String getLDate(){
  String d, m, y;
  String locald;
  if(timeinfo.tm_mday < 10) d = "0" + String(timeinfo.tm_mday);
  else d = String(timeinfo.tm_mday);

  if(timeinfo.tm_mon < 10) m = "0" + String(1+timeinfo.tm_mon);
  else m = String(1+timeinfo.tm_mon);
  
  if(timeinfo.tm_year < 10) y = "0" + String(1900+timeinfo.tm_year);
  else y = String(1900+timeinfo.tm_year);
 
  locald = y +"-"+ m +"-"+ d; 
  return locald;
}

String getLTime(){
  getLocalTime(&timeinfo);
  return getLDate() + " " + getLHour();
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

    


