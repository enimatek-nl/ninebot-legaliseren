#include <Arduino.h>
#include <SoftwareSerial.h>

// Segway Ninebot ES1LD
// Author: github.com/enimatek-nl

// Header: BYTE 1 & 2
#define NINEBOT_H1 0x5A
#define NINEBOT_H2 0xA5
// Source: BYTE 3 & Destination: BYTE 4 (NINEBOT)
#define WIRED_SERIAL 0x3D
#define BLUETOOTH_SERIAL 0x3E
#define UNKNOWN_SERIAL 0x3F
// Command: BYTE 4 (NINEBOT)
#define SPEED 0x64 // ONLY AT DESTINATION 0x21 ESC>BLE

const String VERSION = "2.3";
bool DEBUG_MODE = false;                            // Enable debug mode - print details through serial-output
const int SERIAL_PIN = 2;                           // Pin of serial RX line from ESC
const int THROTTLE_PIN = 10;                        // PWM pin of throttle
const int LED0_PIN = 13;                            // Communicate some info through a led
const int SPEED_HIST_SIZE = 20;                     // amount of data points in history
const int SPEED_MIN = 5;                            // Minium speed to activate/deactivate throttle
const int SPEED_MAX = 17;                           // Max speed before max throttle is needed (max-specs - 3km/h?)
const int KICKS_RESET_TRESHOLD = 1;                 // After X kicks next one will reset speed-building lock
const int THROTTLE_STOP = 45;                       // Stop value for throttle (45 anti KERS).
const int THROTTLE_MIN = 90;                        // Min value for first boost (±7km/h 110?)
const int THROTTLE_STEP = 10;                       // Each step until SPEED_MAX
const int THROTTLE_MAX = 233;                       // Max known value for throttle (233 most cases)
const int THROTTLE_TUNE = 5;                        // tuning value for dynamic dec-/inc-rease of throttle based on targetSpeed
const unsigned long DELAY_KICK_DETECTION = 350;     // delay between kick detections
const unsigned long DELAY_KEEP_THROTTLE = 20000;    // time to keep the throttle open after kick
const unsigned long DELAY_STOPPING_THROTTLE = 2000; // time between decrease of throttle when no kick is given
const unsigned long DELAY_SPEED_DETECTION = 2000;   // prevent target speed changes during kicks

//
// Start of actual code, don't change stuff below this line when in doubt...
//

uint8_t serialHeader1 = 0x00;
uint8_t serialHeader2 = 0x00;
uint8_t buff[256];   // Reserved for serial packet reading
int pSizeOffset = 4; // Packet offset
int pSpeed = 0;      // Packet speed

bool pauseKickDetection = false;
int intermediateKicks = 0; // buffer during speed increase to prevent fake kicks

unsigned long currentTime = 0;
unsigned long kickResetTimer = 0;
unsigned long throttleStopTimer = 0;
unsigned long speedTargetUnlockTimer = 0;
unsigned long nextBrakeTimer = 0;

int averageSpeed = 0;
int targetSpeed = 0;

int speedHistTotal = 0;         // Sum of the history.
int speedHist[SPEED_HIST_SIZE]; // History of speeds.
int speedHistIndex = 0;         // Current index in the history.

SoftwareSerial SoftSerial(SERIAL_PIN, 3); // RX, TX

void setup()
{
    if (DEBUG_MODE)
        Serial.begin(115200);

    SoftSerial.begin(9600); // 9600 baud voor dit model

    if (DEBUG_MODE)
        Serial.println("ES1LD Firmware " + VERSION + " Ready.");

    TCCR1B = TCCR1B & 0b11111001; // PIN 9 & 10 to 32 khz PWM

    pinMode(LED0_PIN, OUTPUT);
    digitalWrite(LED0_PIN, LOW);

    analogWrite(THROTTLE_PIN, THROTTLE_STOP);
}

uint8_t readByte()
{
    while (!SoftSerial.available())
        delay(1);
    return SoftSerial.read();
}

void loop()
{
    if (serialHeader1 == 0x00)
    {
        switch (readByte())
        {
        case NINEBOT_H1:
            if (readByte() == NINEBOT_H2)
            {
                if (DEBUG_MODE)
                    Serial.println("!! Detected NineBot Protocol");
                serialHeader1 = NINEBOT_H1;
                serialHeader2 = NINEBOT_H2;
            }
            else
            {
                if (DEBUG_MODE)
                    Serial.println("?? Unknown NineBot Protocol");
            }
            break;
        default:
            if (DEBUG_MODE)
                Serial.println("!! Detected Unknown Protocol");
            break;
        }
    }
    else
    {
        while (readByte() != serialHeader1)
            ; // Waiting for header1
        if (readByte() != serialHeader2)
            return;                 // Second byte does not match
        uint8_t pSize = readByte(); // Byte 3 = packet size
        if (pSize < 3 || pSize > 8)
        {
            if (DEBUG_MODE)
                Serial.println((String) "!! Incorrect pSize of " + pSize);
            return; // Incorrect packet size
        }
        buff[0] = pSize;
        uint8_t r = 0x00;
        uint16_t sum = pSize;
        for (int i = 0; i < pSize + pSizeOffset; i++)
        {
            r = readByte();
            buff[i + 1] = r;
            sum += r;
        }
        uint16_t checksum = (uint16_t)readByte() | ((uint16_t)readByte() << 8); // Last 2 bytes contain checksum
        if (checksum != (sum ^ 0xFFFF))
        {
            if (DEBUG_MODE)
                Serial.println((String) "!! Invalid checksum: " + checksum + " != " + (sum ^ 0xFFFF));
            return;
        }

        else if (buff[2] == 0x21 && buff[3] == SPEED)
        {
            pSpeed = buff[pSize + pSizeOffset - 1];
            speedHistTotal = speedHistTotal - speedHist[speedHistIndex];
            speedHist[speedHistIndex] = pSpeed;
            speedHistTotal = speedHistTotal + speedHist[speedHistIndex];
            speedHistIndex = speedHistIndex + 1;
            if (speedHistIndex >= SPEED_HIST_SIZE)
            {
                speedHistIndex = 0;
            }
            averageSpeed = speedHistTotal / SPEED_HIST_SIZE;

            if (pSpeed != 0 && pSpeed < SPEED_MIN)
            {
                targetSpeed = 0; // Going to slow or brake, disable throttle
            }
            else
            {
                kickDetection();
            }

            if (DEBUG_MODE && pSpeed != 0)
                Serial.println((String) "?? pSpeed changed: " + pSpeed + ", Avg: " + averageSpeed + ", kickPaused:" + pauseKickDetection);
        }

        else
        {
            if (buff[3] != SPEED)
            {
                if (DEBUG_MODE)
                {
                    Serial.println("!! Unknown packet:");
                    for (int i = 0; i < pSize + pSizeOffset; i++)
                    {
                        Serial.print(buff[i], HEX);
                        Serial.print(" ");
                    }
                    Serial.println("END.");
                }
            }
        }

        analogWrite(THROTTLE_PIN, calcThrottle());

        // Timers... without heavy lib
        currentTime = millis();
        if (kickResetTimer != 0 && kickResetTimer + DELAY_KICK_DETECTION < currentTime)
            resetKickDetection();
        if (throttleStopTimer != 0 && throttleStopTimer + DELAY_KEEP_THROTTLE < currentTime)
            stopThrottle();
        if (speedTargetUnlockTimer != 0 && speedTargetUnlockTimer + DELAY_SPEED_DETECTION < currentTime)
            unlockSpeedTarget();
        if (nextBrakeTimer != 0 && nextBrakeTimer + DELAY_STOPPING_THROTTLE < currentTime)
            brakeThrottle();
    }
}

void kickDetection()
{
    int tier = calcTier(); // Calculate tier based on current speed, starting speed is higher to prevent differences.

    if (averageSpeed > SPEED_MIN && (pSpeed - averageSpeed) >= tier && !pauseKickDetection)
    {
        if (speedTargetUnlockTimer == 0 || intermediateKicks >= KICKS_RESET_TRESHOLD)
        {
            targetSpeed = pSpeed >= SPEED_MAX ? SPEED_MAX : pSpeed;

            if (DEBUG_MODE)
                Serial.println((String) "!! Kick Detected, current step speed: " + pSpeed + ", target speed: " + targetSpeed);

            digitalWrite(LED0_PIN, HIGH);
            intermediateKicks = 0;
            pauseKickDetection = true;
            nextBrakeTimer = 0;
            kickResetTimer = currentTime;
            speedTargetUnlockTimer = currentTime;
            throttleStopTimer = currentTime;
        }
        else
        {
            intermediateKicks++; // increase kick counter and based on that we slowly increase the target speed?
        }
    }
    else if (averageSpeed > SPEED_MIN && pSpeed < averageSpeed)
    {
        // TODO: some magic detection of brake and/or hill?
    }
}

// Dynamically allocate throttle based on the speed instead of the throttle itself.
int calcThrottle()
{
    if (targetSpeed > SPEED_MIN)
    {
        if (targetSpeed >= SPEED_MAX)
        {
            return THROTTLE_MAX;
        }
        else
        {
            int d = (targetSpeed - averageSpeed);
            d = d * THROTTLE_TUNE; // small increase (or decrease) the throttle based on current speed compared to the average
            int t = (THROTTLE_MIN + d) + (THROTTLE_STEP * (targetSpeed - SPEED_MIN));
            return t > THROTTLE_MAX ? THROTTLE_MAX : t;
        }
    }
    else
    {
        return THROTTLE_STOP;
    }
}

int calcTier()
{
    if (averageSpeed > 14)
        return 1;
    if (averageSpeed >= 5)
        return 2;
    return 3;
}

void unlockSpeedTarget()
{
    speedTargetUnlockTimer = 0;
    digitalWrite(LED0_PIN, LOW);
    intermediateKicks = 0;
    if (DEBUG_MODE)
        Serial.println("!! Speed Target unlocked, resetting kickcounter.");
}

void resetKickDetection()
{
    kickResetTimer = 0;
    pauseKickDetection = false;
    if (DEBUG_MODE)
        Serial.println("!! Kick Detection resumed.");
}

void stopThrottle()
{
    nextBrakeTimer = currentTime;
    throttleStopTimer = 0;
    if (DEBUG_MODE)
        Serial.println("!! Throttle braking, time for a kick.");
}

void brakeThrottle()
{
    targetSpeed--;
    if (targetSpeed <= SPEED_MIN)
    {
        targetSpeed = 0;
        nextBrakeTimer = 0;
    }
    else
    {
        nextBrakeTimer = currentTime;
    }
    if (DEBUG_MODE)
        Serial.println((String) "!! Throttle braking, targetSpeed: " + targetSpeed);
}
