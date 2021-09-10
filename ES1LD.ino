#include <Arduino.h>
#include <SoftwareSerial.h>

// Segway Ninebot ES1LD
// Author: github.com/enimatek-nl

// the apped speed is for 'S' mode
// error? add: "__AVR_ATmega328__",

#define NINEBOT_H1 0x5A
#define NINEBOT_H2 0xA5
#define SPEED 0x64

const String VERSION = "1.0";
bool DEBUG_MODE = false; // Enable debug mode - print details through serial-output

const int KICKS_RESET_TRESHOLD = 1;                 // After X kicks next one will reset speed-building lock
const unsigned long DELAY_KICK_DETECTION = 350;     // delay between setting a kick detection speed target
const unsigned long DELAY_KEEP_THROTTLE = 9000;     // time to keep the throttle open after kick
const unsigned long DELAY_STOPPING_THROTTLE = 1000; // time between decrease of throttle when no kick is given
const unsigned long DELAY_SPEED_DETECTION = 2500;   // prevent target speed changes during kicks

const int THROTTLE_PIN = 10;    // PWM pin of throttle
const int SPEED_HIST_SIZE = 20; // amount of data points in history
const int SPEED_MIN = 5;        // Minium speed to activate/deactivate throttle
const int SPEED_MAX = 20;       // size of the speed_map array (0 to 22 km/h)
const int THROTTLE_STOP = 45;   // Stop value for throttle (45 anti KERS).
const int THROTTLE_TUNE = 2;    // Tune throttle when hitting a hill, or to boost quicker
const int THROTTLE_STEP = 8;    // Each increase in throttle between min and max
const int THROTTLE_MIN = 80;    // Min throttle (= min speed)
const int THROTTLE_MAX = 230;   // max throttle (= max speed)

//
// Start of actual code, don't change stuff below this line when in doubt...
//
uint8_t pBuff[256];  // Packet buffer
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

SoftwareSerial SoftSerial(2, 3); // RX, TX

void setup()
{
    if (DEBUG_MODE)
        Serial.begin(115200);

    SoftSerial.begin(9600); // 9600 baud voor dit model

    if (DEBUG_MODE)
        Serial.println("ES1LD Firmware " + VERSION + " Ready.");

    TCCR1B = TCCR1B & 0b11111001; // PIN 9 & 10 to 32 khz PWM

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

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
    while (readByte() != NINEBOT_H1)
        ; // Waiting for header1
    if (readByte() != NINEBOT_H2)
        return;                 // Second byte does not match
    uint8_t pSize = readByte(); // Byte 3 = packet size
    if (pSize < 3 || pSize > 8)
    {
        if (DEBUG_MODE)
            Serial.println((String) "!! Incorrect pSize of " + pSize);
        return; // Incorrect packet size
    }
    pBuff[0] = pSize;
    uint8_t r = 0x00;
    uint16_t sum = pSize;
    for (int i = 0; i < pSize + pSizeOffset; i++)
    {
        r = readByte();
        pBuff[i + 1] = r;
        sum += r;
    }
    uint16_t checksum = (uint16_t)readByte() | ((uint16_t)readByte() << 8); // Last 2 bytes contain checksum
    if (checksum != (sum ^ 0xFFFF))
    {
        if (DEBUG_MODE)
            Serial.println((String) "!! Invalid checksum: " + checksum + " != " + (sum ^ 0xFFFF));
        return;
    }

    else if (pBuff[2] == 0x21 && pBuff[3] == SPEED)
    {
        pSpeed = pBuff[pSize + pSizeOffset - 1];
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
            targetSpeed = 0;
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
        if (pBuff[3] != SPEED)
        {
            if (DEBUG_MODE)
            {
                Serial.println("!! Unknown packet:");
                for (int i = 0; i < pSize + pSizeOffset; i++)
                {
                    Serial.print(pBuff[i], HEX);
                    Serial.print(" ");
                }
                Serial.println("END.");
            }
        }
    }

    analogWrite(THROTTLE_PIN, calcThrottle());

    // Timers.
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

void kickDetection()
{
    int tier = calcTier(); // Calculate tier based on current speed, starting speed is higher to prevent differences.

    if (averageSpeed > SPEED_MIN && (pSpeed - averageSpeed) >= tier && !pauseKickDetection)
    {
        if (speedTargetUnlockTimer == 0 || intermediateKicks >= KICKS_RESET_TRESHOLD)
        {
            if (DEBUG_MODE)
                Serial.println((String) "!! Kick Detected, current step speed: " + pSpeed + ", target speed: " + targetSpeed);
            digitalWrite(LED_BUILTIN, HIGH);
            //targetSpeed = pSpeed >= SPEED_MAX ? SPEED_MAX : pSpeed;
            targetSpeed = pSpeed;
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
    else if (averageSpeed > SPEED_MIN && speedTargetUnlockTimer == 0 && pSpeed < averageSpeed)
    {
        // TODO: some magic detection of brake and/or hill?
    }
}

int calcTier()
{
    if (averageSpeed > 14)
        return 1;
    if (averageSpeed >= SPEED_MIN)
        return 2;
    return 3;
}

void unlockSpeedTarget()
{
    if (DEBUG_MODE)
        Serial.println((String) "!! Speed Target unlocked, needed: " + targetSpeed + ", reached: " + pSpeed + " : " + averageSpeed + " km/h  @ " + calcThrottle() + " throttle.");
    digitalWrite(LED_BUILTIN, LOW);
    speedTargetUnlockTimer = 0;
    intermediateKicks = 0;
}

void resetKickDetection()
{
    if (pSpeed > targetSpeed)
        targetSpeed = pSpeed;
    kickResetTimer = 0;
    pauseKickDetection = false;
    if (DEBUG_MODE)
        Serial.println("!! Kick Detection resumed.");
}

void stopThrottle()
{
    throttleStopTimer = 0;
    nextBrakeTimer = currentTime;
    if (DEBUG_MODE)
        Serial.println("!! Throttle starting to brake, time for a kick?");
}

void brakeThrottle()
{
    targetSpeed--;
    if (targetSpeed < SPEED_MIN)
    {
        targetSpeed = 0;
        nextBrakeTimer = 0;
    }
    else
    {
        nextBrakeTimer = currentTime;
    }
    if (DEBUG_MODE)
        Serial.println((String) "!! Throttle braking, targetSpeed now: " + targetSpeed);
}

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

            int d = 0;
            if (speedTargetUnlockTimer == 0)
                d = (targetSpeed - averageSpeed) * (THROTTLE_TUNE * 2);
            int z = (targetSpeed - ((SPEED_MIN + SPEED_MAX) / 2)) * THROTTLE_TUNE;
            d = d + z > 0 ? z : 0;
            int t = (THROTTLE_MIN + d) + (THROTTLE_STEP * (targetSpeed - SPEED_MIN));
            return t > THROTTLE_MAX ? THROTTLE_MAX : t;
        }
    }
    else
    {
        return THROTTLE_STOP;
    }
}
