/*
Many thanks to nikxha from the ESP8266 forum
*/
#include <Arduino.h>
#include <SPI.h>
#include "MFRC522.h"
#include <LiquidCrystal.h>
#include <Keypad.h>

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/

//defining pins
#define RST_PIN  9  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5
#define SS_PIN  10  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4
#define TRAVA 15  //door lock pin

//init keyboard consts
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

//init keyboard pins
byte rowPins[ROWS] = {2, 1, 0, A5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {A4, A3, A2, A1}; //connect to the column pinouts of the keypad

//init instances
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
LiquidCrystal lcd(4, 3, 8, 7, 6, 5);  //Create LCD instance

//estado da trava
int tr_dest = 1;

//estado do botão do keypad shield
int OS_button = 1;
int state = 0;

//controle dos cartões
int savecard = 0;
int num_card;
String saved_cards[20];

//senha
char saved_password[6];
char trial_password[6];
int passwd_false = 0;

void setup() {
  saved_cards[0] = "F9 EC DE 2B";
  num_card = 0;
  lcd.begin(16, 2);

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked

  Serial.begin(9600);    // Initialize serial communications
  delay(250);

  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522

  delay(3000);
}

void loop() {
  //init password
  while(!passwd_false){
    mensagemSenha();


    for(int i = 0; i<7; i++){
      char key = keypad.getKey();
      while(key == NO_KEY){
        key = keypad.getKey();
      }

      if(i !=0)
      {
        lcd.setCursor(i-1, 1);
        lcd.print('*');
        saved_password[i-1] = key;
      }
    }
    delay(1000);

    for(int i = 0; i<6; i++){
      Serial.print(saved_password[i]);
    }

    int buttonP = 0;
    Serial.println(buttonP);
    mensagemConfirmaSenha();
    while(!(buttonP = buttonRead()) ){

    }
    if(buttonP == 1){
      passwd_false = 1;
      mensagemSenhaSalva();
      delay(1000);
      state = 0;
    }
    else{
      for(int i = 0; i<6 ;i++){
        saved_password[i] = NULL;
        state = 0;
      }
    }
  }
  int buttonCS = 0;
  Serial.print(buttonCS);
  mensagemSenhaOuCartao();
  while(!(buttonCS = buttonRead())){

  }
  while(buttonCS == 1){
    mensagemSenha2();
    delay(1000);
    for(int i = 0; i<6; i++){
      char key = keypad.getKey();
      while(key == NO_KEY){
        key = keypad.getKey();
      }

      lcd.setCursor(i, 1);
      lcd.print('*');
      trial_password[i] = key;
    }
    delay(1000);

    int not_match_passwd = 0;
    for(int i = 0; i<6; i++){
      if(trial_password[i] != saved_password[i]) not_match_passwd = 1;
    }

    if(not_match_passwd == 1){
      mensagemSenhaIncorreta();
      delay(1000);
    }
    else{
      if (tr_dest == 1) {
          tr_dest = 0; //door unlocked
          digitalWrite(TRAVA, HIGH);
          mensagemEntradaLiberada();
      }
      //Unlocked door, lock it.
      else {
        tr_dest = 1; //door locked
        digitalWrite(TRAVA, LOW);
        mensagemPortaTravada();
      }
    }

    buttonCS = 0;
    state = 0;
    Serial.print(buttonCS);
  }
  while(buttonCS == 2){
    state = 0;
    //choose between read or write card
    int buttonRW = 0;
    Serial.print(buttonRW);
    mensagemInicial();
    delay(1000);
    while(!(buttonRW = buttonRead())){

    }
    //WRITE

    while(buttonRW == 1){
      mensagemLeituraCartaoEscrita();
      delay(1000);

      while( ! mfrc522.PICC_IsNewCardPresent()) {
        delay(50);
        //return;
      }
      // Select one of the cards
      while( ! mfrc522.PICC_ReadCardSerial()) {
        delay(50);
        //return;
      }
      delay(3000);
      Serial.print("AQUI");
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
      Serial.println(F("Message : "));
      content.toUpperCase();

      String card = content.substring(1);
      Serial.println(card);

      mensagemSalvar();
      state = 0;
      while(!(savecard = buttonRead())){

      }
      if(savecard == 1){
        mensagemSenha2();
        for(int i = 0; i<6; i++){
          char key = keypad.getKey();
          while(key == NO_KEY){
            key = keypad.getKey();
          }

          lcd.setCursor(i, 1);
          lcd.print('*');
          trial_password[i] = key;
        }
        delay(1000);

        int not_match_passwd = 0;
        for(int i = 0; i<6; i++){
          if(trial_password[i] != saved_password[i]) not_match_passwd = 1;
        }

        if(not_match_passwd == 1){
          mensagemSenhaIncorreta();
          delay(1000);
        }
        else{
          saved_cards[num_card] = card;
          num_card++;
          mensagemCartaoSalvo();
          lcd.setCursor(0, 1);
          lcd.print(card);
          delay(2000);
        }
        Serial.println(state);
      }
      state = 0;
      buttonRW = 0;
    }
    //READ
    while(buttonRW == 2){
      mensagemLeituraCartaoLeitura();
      while(! mfrc522.PICC_IsNewCardPresent()){
        delay(50);
        //return;
      }
      while ( ! mfrc522.PICC_ReadCardSerial()) {
        delay(50);
        //return;
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
      Serial.println(F("Message : "));
      content.toUpperCase();

      String card = content.substring(1);
      Serial.println(card);

      //unlock if any of the cards saved were used
      int auth = 0; //acesso autorizado
      for(int i = 0; i < num_card; i++){
        if (saved_cards[i] == card)//compara cartões salvos com cartão lido
        {
          auth = 1;
        }
      }
      if(auth == 1){
        //Locked door, unlock it
        if (tr_dest == 1) {
            tr_dest = 0; //door unlocked
            digitalWrite(TRAVA, HIGH);
            mensagemEntradaLiberada();
        }
        //Unlocked door, lock it.
        else {
          tr_dest = 1; //door locked
          digitalWrite(TRAVA, LOW);
          mensagemPortaTravada();
        }
      }
      else {
        mensagemAcaoNegada();
      }
      state = 0;
      buttonRW = 0;
    }
    while(buttonRW == 3){
      state = 0;
      buttonCS = 0;
      buttonRW = 0;
    }
  }
}

// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//read msg
void mensagemInicial() {
  Serial.println(F("Escrever ou ler cartão?"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Escrever ou");
  lcd.setCursor(0, 1);
  lcd.print("ler cartao?");

}

//read msg
void mensagemLeituraCartaoEscrita() {
  Serial.println(F("Aprox. seu cartão p/ escrita."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aprox. cartao do");
  lcd.setCursor(0, 1);
  lcd.print("leitor p/ escrita");
}

//read msg
void mensagemLeituraCartaoLeitura() {
  Serial.println(F("Aprox. cartão p/ acesso."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aprox. cartao do");
  lcd.setCursor(0, 1);
  lcd.print("leitor p/ acesso");
}

//write msg
void mensagemSalvar() {
  Serial.println(F("Escrever este cartão?"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Salvar cartao?");
}

//card saved msg
void mensagemCartaoSalvo() {
  Serial.println(F("Cartão salvo."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cartao salvo.");
}

//card saved msg
void mensagemSenha() {
  Serial.println(F("Digite uma senha:"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Digite uma senha");
}

void mensagemSenha2() {
  Serial.println(F("Digite a senha:"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Digite a senha");
}

void mensagemConfirmaSenha() {
  Serial.println(F("Confirma senha?"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirma senha?");
}

void mensagemSenhaSalva() {
  Serial.println(F("Senha salva."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Senha salva.");
}

void mensagemSenhaIncorreta() {
  Serial.println(F("Senha incorreta."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Senha incorreta.");
}

void mensagemSenhaOuCartao() {
  Serial.println(F("Senha ou cartão?"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Senha ou cartao?");
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
//ler botao keypadshield
int buttonRead(){
  int x;
  x = analogRead (0);

  if (x < 100) {

    state = 3;
  }

  else if (x < 200) {

    state = 1;
  }
  else if (x < 400){

    state = 2;
  }
  if(state != OS_button){
    Serial.println(state);
  }
  OS_button = state;
  return state;
}
