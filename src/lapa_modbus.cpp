#include <Modbus.h>

//////////////// registers of LIGHTS ///////////////////
enum {
    // The first register starts at address 0
            ACTIONS,      // Always present, used for incoming actions

    // Any registered events, denoted by 'triggered_by_register' in rs485_node of Lua script, 1 and up
            TOTAL_ERRORS     // leave this one, error counter
};

#include <ModbusSerial.h>

ModbusSerial mb;

//#define SSerialGND       10 - not used, direct power
#define SSerialRX        9  //Serial Receive pin
#define SSerialTX        8  //Serial Transmit pin
//#define SSerialVCC       7 - not used, direct power
#define SSerialTxControl (-1) // was 6   //RS485 Direction control, not used in new modbus chip
SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

// Action handler. Add all your actions mapped by action_id in rs485_node of Lua script
void process_actions() {
    if (mb.Hreg(ACTIONS) == 0)
        return;

    switch (mb.Hreg(ACTIONS)) {
        case 1 : // Put here code for Reset
            Serial.println("[Reset] action fired");
//            fail();
            break;
        case 2 : // Put here code for Complete
            Serial.println("[Complete] action fired");
//            success();
            break;
        default:
            break;
    }

    // Signal that action was processed
    mb.Hreg(ACTIONS, 0);
}

void modbus_set(word event, word value) {
    mb.Hreg(event, value);
}

void modbus_setup() {
    Serial.println("ModBus Slave LAPA:8 for lua/Aliens.lua");

#ifdef EMULATE_RS3485_POWER_PINS
    pinMode(SSerialVCC, OUTPUT);
    digitalWrite(SSerialVCC, HIGH);
    pinMode(SSerialGND, OUTPUT);
    digitalWrite(SSerialGND, LOW);
    delay(10);
#endif

    mb.config(&RS485Serial, 31250, SSerialTxControl);
    mb.setSlaveId(8);

    mb.addHreg(ACTIONS, 0);
    pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output (D4)
}

void modbus_loop() {
    mb.task();              // not implemented yet: mb.Hreg(TOTAL_ERRORS, mb.task());
    process_actions();
}
