# ESP-015 OLED Crypto Display Project
A compact and autonomous project using an ESP-015 module and an SSD1306 OLED screen to display real-time prices and 24-hour changes for Bitcoin (BTC), Ethereum (ETH), and Binance Coin (BNB), as well as the current time. WiFi configuration is handled easily using WiFiManager.

## Features
Real-time Price Display: Fetches and shows BTC, ETH, and BNB prices in USD from the CoinGecko API.
24-hour Price Change: Indicates the percentage change in prices over the last 24 hours.
NTP Time Synchronization: Displays the accurate current time, synchronized via NTP.
Page Rotation: Information (time, BTC, ETH, BNB) automatically cycles every 5 seconds.
Easy WiFi Configuration: Uses WiFiManager for intuitive WiFi setup via a captive portal, no code modification needed.
128x64 OLED Screen: Utilizes a compact OLED screen for clear display.
ESP-015 Optimized: Designed to run on the small yet powerful ESP-015 module.

## Hardware Requirements
ESP-015.
SSD1306 OLED Display Module (128x64 pixels, I2C interface)
USB to ESP-015 Converter for code uploading.
Jumper Wires

### Connections (ESP-015to OLED SSD1306)
The ESP-015 module has limited GPIO pins. For I2C, we will use:
ESP-015 GPIO0 (SDA pin) <-> OLED SDA
ESP-015 GPIO2 (SCL pin) <-> OLED SCL
ESP-015 VCC <-> OLED VCC (3.3V)
ESP-015 GND <-> OLED GND

Important: The OLED screen must be powered by 3.3V, compatible with the ESP-015. Verify the power requirements of your OLED module.

## Development Environment Setup
1. Arduino IDE
If you haven't already, download and install the Arduino IDE from the official Arduino website.

2. Add ESP8266 Board Support
In the Arduino IDE:

Go to File > Preferences.

In "Additional Boards Manager URLs", add:
http://arduino.esp8266.com/stable/package_esp8266com_index.json

Go to Tools > Board > Boards Manager...

Search for "esp8266" and install the "esp8266 by ESP8266 Community" package.

3. Install Libraries
Install the following libraries via the Library Manager (Sketch > Include Library > Manage Libraries...):
Adafruit GFX Library by Adafruit (basic graphics library)
Adafruit SSD1306 by Adafruit (for SSD1306 OLED displays)
NTPClient by Fabrice Weinberg (for time synchronization)
ArduinoJson by Benoit Blanchon (for parsing JSON data from APIs)
WiFiManager by Tzapu (for easy WiFi management)

Note: The FreeSans12pt7b.h font is included with the Adafruit GFX Library.

### Configuration and Code Upload
Open the .ino file in the Arduino IDE.
Select the Board: Go to Tools > Board and choose Generic ESP8266 Module.
Board Settings: Verify the following settings (common values are suggested, adjust if necessary for your specific ESP-015 or ESP-015S):

Flash Mode: DOUT or QIO
Flash Size: 4MB (FS:1MB OTA:~3MB) (or your module's size)
CPU Frequency: 80 MHz (or 160MHz)
Upload Speed: 115200
Port: Select the serial port corresponding to your USB to ESP-015 converter (e.g., /dev/ttyUSB0 on Linux, COMx on Windows).

Upload the Code: Click the Upload button (the right arrow icon).

ESP-015 Upload Mode:
To put the ESP-015 into flash mode, you will generally need to:
Connect GPIO0 to GND (or hold down the "Flash" button if your converter has one).

Reset the ESP-015 or ESP-015S (by unplugging/replugging power or pressing a "Reset" button if available).

Once the upload starts, you can release GPIO0 (or the "Flash" button).

## Usage
On the first boot after uploading, or if WiFi information is lost:
The OLED screen will display "Connecting to WiFi..." then the WiFiManager's "Configuration Mode", showing the SSID of the access point created by the ESP-015 (defaulting to AutoConnectAP) and the IP address (192.168.4.1). An AP icon will also be displayed.

Connect your phone or computer to the AutoConnectAP WiFi network (or the SSID the ESP-015 displays) using the default password password.
Once connected, a captive portal should open automatically. If not, open your web browser and navigate to 192.168.4.1.
On the portal, select your home WiFi network and enter its password.
The ESP-015 will restart and automatically connect to your configured WiFi network.

The OLED screen will then display the assigned IP address and begin alternating between crypto data and the current time.
The device will update crypto data every 5 minutes and the time continuously (for the time page).

## Credits
Adafruit Libraries (GFX, SSD1306)
NTPClient Library
ArduinoJson Library
WiFiManager Library
CoinGecko API for crypto data.
## License
This project is licensed under the MIT License. You are free to use, modify, and distribute it.
