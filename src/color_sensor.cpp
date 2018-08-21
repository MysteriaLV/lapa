//
// Created by PVeretennikovs on 21-Aug-18.
//

#include "color_sensor.h"
#include <Arduino.h>

int OutPut = 10;//naming pin10 of uno as output

int frequency = 0, r = 0, g = 0, b = 0;


void color_sensor_setup() {
    Serial.begin(115200);
    Serial.println("Start");
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);//PINS 2, 3,4,5 as OUTPUT
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    pinMode(10, INPUT);//PIN 10 as input
    digitalWrite(4, LOW);
    digitalWrite(5, HIGH);//setting frequency selection to 20%
}

void color_sensor_loop() {
    Serial.print("R=");//printing name
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);//setting for RED color sensor
    frequency = pulseIn(OutPut, LOW);//reading frequency
    Serial.print(frequency - r);//printing RED color frequency
    if (!r)r = frequency;
    delay(100);

    Serial.print(" B=");// printing name
    digitalWrite(6, LOW);
    digitalWrite(7, HIGH);// setting for BLUE color sensor
    frequency = pulseIn(OutPut, LOW);// reading frequency
    Serial.print(frequency - b);// printing BLUE color frequency
    if (!b)b = frequency;
    delay(100);

    Serial.print(" G=");// printing name
    digitalWrite(6, HIGH);
    digitalWrite(7, HIGH);// setting for GREEN color sensor
    frequency = pulseIn(OutPut, LOW);// reading frequency
    Serial.println(frequency - g);// printing GREEN color frequency
    if (!g)g = frequency;
    delay(500);
}