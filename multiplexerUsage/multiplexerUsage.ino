/*
 * Copyright (c) 2024, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>

/// count number of elements in an array
#define COUNT_OF(arr) sizeof((arr)) / sizeof((arr)[0])

/// I2c address of multiplexer
#define MULTIPLEXER_ADDRESS 0x70

/// Port numbers where sensors are attached
static uint8_t muxPort[] = {2, 4};

/// string buffer for error messages
static char errorMessage[64];

/// The driver instance that is talking to the sensor
/// As always only one sensor is visible on the i2c bus and it has no state
/// it is sufficient to have only one instance of the SensirionI2CScd4x.
SensirionI2CScd4x scd4x;

/// Print a uint16_t value as hexadecimal number with even number of digits
void printUint16Hex(uint16_t value) {
    Serial.print(value < 4096 ? "0" : "");
    Serial.print(value < 256 ? "0" : "");
    Serial.print(value < 16 ? "0" : "");
    Serial.print(value, HEX);
}

/// Read the sensor serial number and print it to the serial output
void readAndPrintSerialNumber() {
    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    uint16_t error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.println(errorMessage);
    } else {
        Serial.print("Sensor serial-number: 0x");
        printUint16Hex(serial0);
        printUint16Hex(serial1);
        printUint16Hex(serial2);
        Serial.println();
    }
}

/// Select the i2c bus connected to the specified portNr
/// @param portNr port to select
void selectI2cPort(uint8_t portNr) {
    Serial.print("Selecting port nr ");
    Serial.println(portNr);
    Wire.beginTransmission(MULTIPLEXER_ADDRESS);
    size_t written = Wire.write(1 << portNr);
    uint8_t i2c_error = Wire.endTransmission();
    if (written != 1) {
        Serial.print("write error while selecting port");
        return;
    }
    if (i2c_error == 0) {
        Serial.println("Port selection successful");
    } else {
        Serial.print("Error while selecting port:");
        Serial.println(i2c_error);
    }
}

void setup() {

    // initialize the Serial object
    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }

    // initialize the Wire object
    Wire.begin();

    uint16_t error;

    // initialize the scd4x object
    scd4x.begin(Wire);

    // perform additional initialisation steps on the connected sensors:
    // - stop potential running measurements
    // - read and display the serial number
    // - put the sensor into measurement mode
    for (uint8_t i = 0; i < COUNT_OF(muxPort); i++) {
        selectI2cPort(muxPort[i]);
        // stop potentially previously started measurement
        error = scd4x.stopPeriodicMeasurement();
        if (error) {
            Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
            errorToString(error, errorMessage, sizeof(errorMessage));
            Serial.println(errorMessage);
        }

        readAndPrintSerialNumber();

        // Start Measurement
        error = scd4x.startPeriodicMeasurement();
        if (error) {
            Serial.print(
                "Error trying to execute startPeriodicMeasurement(): ");
            errorToString(error, errorMessage, sizeof(errorMessage));
            Serial.println(errorMessage);
        }
    }
    Serial.println("Waiting for first measurement... (5 sec)");
    delay(5000);
}

void loop() {
    uint16_t error;

    // the sensor requires some delay between two successive reads
    delay(200);

    // read out the measurement values of all connected sensors
    for (uint8_t i = 0; i < COUNT_OF(muxPort); i++) {

        selectI2cPort(muxPort[i]);

        // variables for measurment data
        uint16_t co2 = 0;
        float temperature = 0.0f;
        float humidity = 0.0f;

        // await data ready
        bool isDataReady = false;
        while (true) {

            error = scd4x.getDataReadyFlag(isDataReady);
            if (error) {
                Serial.print("Error trying to execute getDataReadyFlag(): ");
                errorToString(error, errorMessage, sizeof(errorMessage));
                Serial.println(errorMessage);
            }
            if (isDataReady) {
                break;
            }
            delay(10);  // have a short delay between two successive calls to
                        // getDataReadyFlag
        }

        // read the sensor data c02, temperature and humidity
        error = scd4x.readMeasurement(co2, temperature, humidity);
        if (error) {
            Serial.print("Error trying to execute readMeasurement(): ");
            errorToString(error, errorMessage, sizeof(errorMessage));
            Serial.println(errorMessage);
        } else if (co2 == 0) {
            Serial.println("Invalid sample detected, skipping.");
        } else {
            // output the data in case of successful read
            Serial.print("Port: ");
            Serial.print(muxPort[i]);
            Serial.print("\t");
            Serial.print("Co2: ");
            Serial.print(co2);
            Serial.print("\t");
            Serial.print("Temperature: ");
            Serial.print(temperature);
            Serial.print("\t");
            Serial.print("Humidity: ");
            Serial.println(humidity);
        }
    }
}
