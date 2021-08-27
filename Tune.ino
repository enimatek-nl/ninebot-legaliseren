#include <Arduino.h>
#include <SoftwareSerial.h>

// Segway Ninebot ES1LD
// Author: github.com/enimatek-nl
// Summary: This firmware will run the scooter with some intervals to log the throttle vs the km/h
//          The data can be used to define / tune the firmware based on speed settings.

#define NINEBOT_H1 0x5A
#define NINEBOT_H2 0xA5

const int SERIAL_PIN = 2;
const int THROTTLE_PIN = 10;
const int THROTTLE_INCREASE = 5;
const int MAX_THROTTLE = 220;
const unsigned long DELAY_LOG_TIMER = 2000;

SoftwareSerial SoftSerial(SERIAL_PIN, 3); // RX, TX
uint8_t serialBuffer[256];
unsigned long currentTime = 0;
unsigned long logTimer = 0;
int pSizeOffset = 4;
int pSpeed = 0;
int throttle = 45;
int state = 0;

void setup()
{
    Serial.begin(115200);
    SoftSerial.begin(9600);
    TCCR1B = TCCR1B & 0b11111001;
    analogWrite(THROTTLE_PIN, throttle);
    Serial.println((String) "!! Tune Throttle is ready.");
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
        ;
    if (readByte() != NINEBOT_H2)
        return;
    uint8_t pSize = readByte();
    if (pSize < 3 || pSize > 8)
        return;
    serialBuffer[0] = pSize;
    uint8_t r = 0x00;
    uint16_t sum = pSize;
    for (int i = 0; i < pSize + pSizeOffset; i++)
    {
        r = readByte();
        serialBuffer[i + 1] = r;
        sum += r;
    }
    uint16_t checksum = (uint16_t)readByte() | ((uint16_t)readByte() << 8);
    if (checksum != (sum ^ 0xFFFF))
        return;
    else if (serialBuffer[2] == 0x21 && serialBuffer[3] == 0x64)
    {
        pSpeed = serialBuffer[pSize + pSizeOffset - 1];
    }

    analogWrite(THROTTLE_PIN, throttle);

    if (pSpeed > 5 && state == 0 && logTimer == 0)
    {
        state = 1;
        logTimer = millis() + DELAY_LOG_TIMER + 1;
        throttle = 80;
    }

    currentTime = millis();
    if (logTimer != 0 && throttle != 0 && logTimer + DELAY_LOG_TIMER < currentTime)
        logSerial();
}

void logSerial()
{

    Serial.println((String)throttle + " @ " + pSpeed + " km/h");
    throttle += THROTTLE_INCREASE;
    logTimer = currentTime;
    if (throttle > MAX_THROTTLE)
    {
        Serial.println((String) "!! Throttle is maxxed - stopping timer.");
        logTimer = 0;
        throttle = 0;
    }
}
