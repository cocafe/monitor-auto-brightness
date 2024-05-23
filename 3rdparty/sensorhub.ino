#include <stdio.h>
#include <string.h>

#include <Wire.h>
#include <Arduino.h>

#include <BH1750_WE.h>
#include <BMP280.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_SHT31.h>

#define SerialPort Serial1

#define I2C_ADDR_BH1750 0x23
#define I2C_ADDR_BMP280 0x76
#define I2C_ADDR_SHT31  0x44

BH1750_WE bh1750 = BH1750_WE(I2C_ADDR_BH1750);
BMP280 bmp280(I2C_ADDR_BMP280);
Adafruit_SGP30 sgp30;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
static uint32_t getAbsoluteHumidity(float temperature, float humidity) {
        // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
        const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
        const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
        return absoluteHumidityScaled;
}

static void err_trap(void)
{
        while (1);
}

static void __i2cdetect(uint8_t first, uint8_t last) {
        uint8_t i, address, error;
        char buff[10];

        // table header
        SerialPort.print("   ");
        for (i = 0; i < 16; i++) {
                sprintf(buff, "%3x", i);
                SerialPort.print(buff);
        }

        SerialPort.println();

        // table body
        // addresses 0x00 through 0x77
        for (address = 0; address <= 119; address++) {
                if (address % 16 == 0) {
                        //SerialPort.printf("\n%#02x:", address & 0xF0);
                        SerialPort.println();
                        sprintf(buff, "%02x:", address & 0xF0);
                        SerialPort.print(buff);
                }
                if (address >= first && address <= last) {
                        Wire.beginTransmission(address);
                        error = Wire.endTransmission();
                        if (error == 0) {
                                // device found
                                //SerialPort.printf(" %02x", address);
                                sprintf(buff, " %02x", address);
                                SerialPort.print(buff);
                        } else if (error == 4) {
                                // other error
                                SerialPort.print(" XX");
                        } else {
                                // error = 2: received NACK on transmit of address
                                // error = 3: received NACK on transmit of data
                                SerialPort.print(" --");
                        }
                } else {
                        // address not scanned
                        SerialPort.print("   ");
                }
        }
        SerialPort.println("\n");
}

static void i2cdetect(void) {
        SerialPort.println("i2c bus detect:");
        __i2cdetect(0x03, 0x77);  // default range
}

void setup()
{
        SerialPort.begin(115200);
        delay(10);

        Wire.begin();
        delay(1000);

        if (!bh1750.init()) {
                printf("failed to init bh1750\n");
                err_trap();
        }

        if (bmp280.begin() != 0) {
                printf("failed to init bmp280\n");
                err_trap();
        }

        sgp30.begin();

        printf("SGP30 SN: #%02x%02x%02x\n",
               sgp30.serialnumber[0],
               sgp30.serialnumber[1],
               sgp30.serialnumber[2]);

        sht31.begin(I2C_ADDR_SHT31);

        // sht31.heater(1);
        printf("sht31 heater: %s\n",
               sht31.isHeaterEnabled() ? "enabled" : "disabled");

        printf("tempC tempC rH Pa lux TVOC_ppb eCO2_ppm H2 Ethanol\n");
}

void sgp_measure(float tempC, float rH)
{
        static uint32_t cnt = 0;

        sgp30.setHumidity(getAbsoluteHumidity(tempC, rH));

        if (!sgp30.IAQmeasure()) {
                printf("failed to measure sgp30 IAQ\n");
        }

        if (!sgp30.IAQmeasureRaw()) {
                printf("failed to measure sgp30 IAQ RAW\n");
        }

        cnt++;
        if (cnt >= 30) {
                uint16_t TVOC_base, eCO2_base;

                if (!sgp30.getIAQBaseline(&eCO2_base, &TVOC_base)) {
                        printf("failed to get sgp30 baseline readings\n");
                } else {
                        printf("sgp30 baseline: eCO2: 0x%02x TVOC: 0x%02x\n", eCO2_base, TVOC_base);
                }

                cnt = 0;
        }
}

void loop()
{
        float lux = bh1750.getLux();
        uint32_t pressure = bmp280.getPressure();
        float tempC_bmp = bmp280.getTemperature();
        float tempC_sht = sht31.readTemperature();
        float rH = sht31.readHumidity();

        sgp_measure(tempC_sht, rH);

        float TVOC_ppb = sgp30.TVOC;
        float eCO2_ppm = sgp30.eCO2;
        float h2 = sgp30.rawH2;
        float ethanol = sgp30.rawEthanol;

        printf("data: %.2f %.2f %.2f %lu %.2f %.2f %.2f %.2f %.2f\n",
               tempC_bmp, tempC_sht, rH, pressure, lux,
               TVOC_ppb, eCO2_ppm, h2, ethanol);

        delay(1000);
}
