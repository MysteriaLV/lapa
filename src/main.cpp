#include "Arduino.h"
#include <SPI.h>
#include "color_sensor.h"
#include "scale.h"

const byte regOut = 10;
const byte regIn = 9;

//Pin connected to ST_CP of 74HC595
int latchPin = 8;
//Pin connected to SH_CP of 74HC595
int clockPin = 12;
////Pin connected to DS of 74HC595
int dataPin = 11;


/*
L -left
R -right
F - forward
B - backward
U - up
D - down

J - joystick
E - terminal
M - motor0
 */

// Joystcik
const byte JL = B00000001;
const byte JR = B00000010;
const byte JF = B00000100;
const byte JB = B00001000;
const byte JD = B00010000;
const byte JU = B00100000;

//Motors and catch
const byte ML = B00000001;
const byte MR = B00000010;
const byte MB = B00000100;
const byte MF = B00001000;
const byte MU = B00010000;
const byte MD = B00100000;
const byte CA = B01000000;

// Termainals (konceviki)
const byte EL = B00000010;
const byte ER = B00000001;
const byte EB = B00001000;
const byte EF = B00000100;
const byte ED = B00100000;
const byte EU = B00010000;

const byte CATCH_NONE = 0;
const byte CATCH_START = 1;
const byte CATCH_U2D = 2;
const byte CATCH_DOWN = 3;
const byte CATCH_D2U = 4;
const byte CATCH_UP = 5;

byte move = 0;
byte catching = 0;
byte catchingStep = CATCH_NONE;
byte joy, end;
byte jState = 0;
byte eState = 0;

unsigned long milli;
unsigned long startCatch;

void spiDo() {
    // PL to LOW -> async load via D0-D7
    digitalWrite(regIn, LOW);
    // PL to HIGH -> shifting and serial loading
    digitalWrite(regIn, HIGH);

    //closing output latch
    digitalWrite(regOut, LOW);

    // write current state & read 1st input register - joystick
    joy = SPI.transfer(move);
    // write current state & read 2nd input register - terminals
    end = SPI.transfer(move);

    // releasing output latch
    digitalWrite(regOut, HIGH);

    joy = ~(joy & B00111111);
    end = ~(end & B00111111);
}

void setup() {

    Serial.begin(115200);
    SPI.begin();
    pinMode(regOut, OUTPUT);
    pinMode(regIn, OUTPUT);

//    TODO
//    color_sensor_setup();
//    scale_setup();


/*
 * Obsolete/unused code
 *
    delay(500);
    spiDo();
    while (!(end & EU)) // move to up
    {
        move = MU;
        spiDo();
        delay(20);
    }

    return;

    //set pins to output so you can control the shift register
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, OUTPUT);

    digitalWrite(latchPin, LOW);
    // shift out the bits:
    shiftOut(dataPin, clockPin, MSBFIRST, 34);

    //take the latch pin high so the LEDs will light up:
    digitalWrite(latchPin, HIGH);
*/
 }

void loop() {
//    color_sensor_loop();
//    scale_loop();

    milli = millis();
    spiDo();

    move = 0;
    if (catchingStep != CATCH_NONE) {
        if (catchingStep == CATCH_U2D) {
            move = MD;
            if (end & ED) {
                Serial.print("CATCH DOWN TO UP");
                catchingStep = CATCH_D2U;
            }
        } else if (catchingStep == CATCH_D2U) {
            move = MU | CA;
            if (end & EU)
                catchingStep = CATCH_UP;
        } else if (catchingStep == CATCH_UP) {
            move = CA;
            if (joy & JU)
                catchingStep = CATCH_NONE;
        }
    }
    if (catchingStep == CATCH_NONE || catchingStep == CATCH_D2U || catchingStep == CATCH_UP) {
        if (joy & JL)move |= ML;
        if (joy & JR)move |= MR;
        if (joy & JF)move |= MF;
        if (joy & JB)move |= MB;

        if (catchingStep == CATCH_NONE && (joy & JD))
            catchingStep = CATCH_U2D;
    }
    if (end & EL)move &= ~ML;
    if (end & ER)move &= ~MR;
    if (end & EF)move &= ~MF;
    if (end & EB)move &= ~MB;
    if (end & ED)move &= ~MD;
    if (end & EU)move &= ~MU;

    if (joy != jState || end != eState) {
        eState = end;
        jState = joy;
        Serial.print("JOY: ");
        if (joy & JL)Serial.print("L");
        if (joy & JR)Serial.print("R");
        if (joy & JF)Serial.print("F");
        if (joy & JB)Serial.print("B");
        if (joy & JU)Serial.print("U");
        if (joy & JD)Serial.print("D");
        Serial.print("\tEND: ");
        if (end & EL)Serial.print("L");
        if (end & ER)Serial.print("R");
        if (end & EF)Serial.print("F");
        if (end & EB)Serial.print("B");
        if (end & EU)Serial.print("U");
        if (end & ED)Serial.print("D");
        Serial.print("\tC-STEP: ");
        Serial.println(catchingStep);
    }
    delay(10);
}