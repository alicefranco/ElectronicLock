#include <Arduino.h>

void setup() {
  // put your setup code here, to run once:
  pinMode(5, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
    digitalWrite(5, 0);
    delay(5000);
    digitalWrite(5, 1);
    delay(5000);
}
