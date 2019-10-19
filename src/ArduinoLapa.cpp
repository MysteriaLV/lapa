#include "Arduino.h"
#include <SPI.h>
#include <NewPing.h>
#include "HX711.h"
#include <Tic.h>

extern void modbus_setup();
extern void modbus_loop();

// ============= sonar ===========================

#define US_TRIGGER_PIN 16
#define US_ECHO_PIN 17
#define US_MAX_DISTANCE 200

NewPing sonar(US_TRIGGER_PIN, US_ECHO_PIN, US_MAX_DISTANCE);

// ============= scales ===========================

HX711 scale(A1, A0);

float tolerance = 10;
float minWeightToDetect = 20;
float expectedItemWeights[4] = {69, 40, 88, 40};
float currentItemLoad[4] = {0, 0, 0, 0};
byte currentItem = 0;
float oldWeightValue = 0;
float currentWeightValue = 0;
float currentWeightDelta = 0;
int calmdownTime = 1500;
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))
unsigned long scaleTiming;
byte secretOpen = 0;

// ============= stepper ===========================

TicI2C tic;
#define DOOR_UNLOAD_DELAY 3000
// initially we assume box door is closed when switching on arduino
#define DOOR_CLOSED_POS 0
#define DOOR_OPENED_POS 3450

// ============= manipulator =======================

const byte regOut = 10;
//const byte regIn = 9;
// D9 changed to D4 after moving from Nano to Uno
const byte regIn = 4;

// D13 - Clock pin 
// D10 latch
// D11 data


// ============= secret door =======================
#define SECRET_RELAY_PIN 3


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
const byte SC = B10000000;

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
byte catchingStep = CATCH_NONE;
byte joy, end;
byte jState = 0;
byte eState = 0;

void spiDo();

void setup();

void loop();

void setupScales();

void loopScales();

void itemDetected(float itemWeight);
void unload();
void reset1();
void setupStepper();
void resetCommandTimeout();
void delayWhileResettingCommandTimeout(uint32_t ms);
void openDoor();
void closeDoor();
void openSecret();
bool checkSolution();

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

// ======================= scale stuff ==========================
// ==============================================================

void setupScales() {
    Serial.println("Setup scales");
    scale.set_scale(2280.f);
    delay(500); // this is for scale to warmup
    scale.tare();
}

void loopScales() {
    Serial.print("Measure:\t");

    currentWeightValue = scale.get_units(2);
    Serial.print("currentWeightValue:\t");
    Serial.println(currentWeightValue, 1);

    if (currentWeightValue < 0 - tolerance) {
        Serial.print("Resetting scale tare:\t");
        scale.tare();
        return;
    }

    // if all items were removed manually();
    if (currentItem > 0 && abs(currentWeightValue) < tolerance) {
        Serial.print("Items removed manually?:\t");
        reset1();
    } else {

        currentWeightDelta = currentWeightValue - oldWeightValue;

        // if weight changed more than tolerance
        if (abs(currentWeightDelta) > tolerance) {
            Serial.print("Weight change detected:\t");
            Serial.print("Calmdown:\t");
            Serial.print(calmdownTime, 1);
            Serial.println("ms:\t");
            delay(calmdownTime);
            currentWeightValue = scale.get_units(10);
            currentWeightDelta = currentWeightValue - oldWeightValue;

            Serial.print("Weight measured raw / delta:\t");
            Serial.print(currentWeightValue, 1);
            Serial.print(" / ");
            Serial.println(currentWeightDelta, 1);


            if (currentWeightDelta > minWeightToDetect) {
                itemDetected(currentWeightDelta);
            }

            oldWeightValue = currentWeightValue;

        }
    }
}

void itemDetected(float itemWeight) {
    Serial.print("Item:\t");
    Serial.print(currentItem, 1);
    Serial.print("\tWeight:\t");
    Serial.println(itemWeight, 1);

    if (currentItem < NUMITEMS(currentItemLoad)) {
        Serial.println("Adding weight to array");
        currentItemLoad[currentItem] = itemWeight;

        Serial.print("Items collected:\t");
        Serial.print(currentItem + 1, 1);
        Serial.print(" of ");
        Serial.println(NUMITEMS(currentItemLoad), 1);

        currentItem++;
    }

    if (currentItem >= NUMITEMS(currentItemLoad)) {
        // check for solution and unload if fail:
        if (checkSolution()) {
            openSecret();
        } else {
            Serial.println("Puzzle failed. Unloading...");
            unload();
        }
    }
}


bool checkSolution() {
    delay(1000);
    Serial.println("Checking solution...");

    bool puzzleSolved = true;
    float delta = 0;
    byte i = 0;
    for (i = 0; i < NUMITEMS(currentItemLoad); i++) {
        delta = currentItemLoad[i] - expectedItemWeights[i];
        if (abs(delta) > tolerance) {
            puzzleSolved = false;
        }
    }

    return puzzleSolved;
}

// upen and unload balls from the box
void unload() {

    reset1();

    openDoor();
    delay(DOOR_UNLOAD_DELAY);
    closeDoor();
}

void reset1() {
    Serial.print("RESET...:\t");
    currentItem = 0;
    byte i = 0;
    for (i = 0; i < NUMITEMS(currentItemLoad); i++) {
        currentItemLoad[i] = 0.0;
    }
    oldWeightValue = 0;
    Serial.println("COMPLETE");
}

// ====================== stpper =======================

void setupStepper() {
    Serial.println("Setup stepper");
    // Set up I2C.
    Wire.begin();

    // Give the Tic some time to start up.
    delay(20);

    // Set the Tic's current position to 0, so that when we command
    // it to move later, it will move a predictable amount.
    tic.haltAndSetPosition(0);

    // Tells the Tic that it is OK to start driving the motor.  The
    // Tic's safe-start feature helps avoid unexpected, accidental
    // movement of the motor: if an error happens, the Tic will not
    // drive the motor again until it receives the Exit Safe Start
    // command.  The safe-start feature can be disbled in the Tic
    // Control Center.
    tic.exitSafeStart();

    closeDoor();
}

// Sends a "Reset command timeout" command to the Tic.  We must
// call this at least once per second, or else a command timeout
// error will happen.  The Tic's default command timeout period
// is 1000 ms, but it can be changed or disabled in the Tic
// Control Center.
void resetCommandTimeout() {
    tic.resetCommandTimeout();
}

// Delays for the specified number of milliseconds while
// resetting the Tic's command timeout so that its movement does
// not get interrupted by errors.
void delayWhileResettingCommandTimeout(uint32_t ms) {
    uint32_t start = millis();
    do {
        resetCommandTimeout();
    } while ((uint32_t) (millis() - start) <= ms);
}

void openDoor() {
    Serial.println("Opening Door");
    tic.setTargetPosition(DOOR_OPENED_POS);
    delayWhileResettingCommandTimeout(1000);
}

void closeDoor() {
    Serial.println("Closing Door");
    tic.setTargetPosition(DOOR_CLOSED_POS);
    delayWhileResettingCommandTimeout(1000);
}

void openSecret() {
    secretOpen = 1;
    Serial.println("===================================================");
    Serial.println("================ OPENING SECRET ===================");
    Serial.println("===================================================");

    digitalWrite(SECRET_RELAY_PIN, LOW);
    delay(1000);
    digitalWrite(SECRET_RELAY_PIN, HIGH);
    /*
    move = SC;
    spiDo();
    delay(2000);
    move = 0;
    spiDo();
    delay(1000);
    */
}

void setup() {
    Serial.begin(115200);
    Serial.println("Setup start");
    modbus_setup();

    pinMode(SECRET_RELAY_PIN, OUTPUT);
    digitalWrite(SECRET_RELAY_PIN, HIGH);


    SPI.begin();
    pinMode(regOut, OUTPUT);
    pinMode(regIn, OUTPUT);

    spiDo();
    setupScales();
    setupStepper();
    unload();
}

void loop() {
    modbus_loop();
    spiDo();

    if (secretOpen == 1) {
        // Serial.print("Secret opened. nothing to do");
        return;
    }

    unsigned int distance = sonar.ping_cm();

    move = 0;
    if (catchingStep != CATCH_NONE) {
        if (catchingStep == CATCH_U2D) {
            Serial.print("CATCH Mode U2D");
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
        //if (joy & JL) {  move = SC; return 0; } // ML;
        if (joy & JL)move |= ML;
        if (joy & JR)move |= MR;
        if (joy & JF)move |= MF;
        if (joy & JB)move |= MB;

        if (catchingStep == CATCH_NONE && (joy & JD) && distance >= 60) {
            catchingStep = CATCH_U2D;
        }
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
        Serial.print(catchingStep);
        Serial.print("\tDISTANCE: ");
        Serial.println(distance);
    }
    delay(50);

// measure scales each second, not each loop
    if (millis() - scaleTiming > 1000) {
        scaleTiming = millis();
        loopScales();
    }
}
