# ESP8266 OLED Crypto Display
This project displays live prices of Bitcoin (BTC), Ethereum (ETH), and Binance Coin (BNB) in USD, along with their 24-hour change, on an OLED screen connected to an ESP8266 microcontroller. It also shows the time synchronized via NTP. WiFi configuration is managed conveniently using the WiFiManager library, allowing you to change networks without recompiling the code.

## Features
Rotating Data Display: Information (time, BTC price, ETH price, BNB price) automatically rotates every 5 seconds.
Real-time Prices: Retrieves current prices and 24-hour changes for cryptocurrencies via the CoinGecko API.
Time Synchronization (NTP): Displays precise time, synchronized with an NTP server.
Easy WiFi Configuration: Uses the WiFiManager library for wireless setup via a captive portal, eliminating the need to hardcode WiFi credentials.
Button-Triggered WiFi Reconfiguration Mode: A long press (3 seconds) on a dedicated GPIO pin (default GPIO0/D3) forces the ESP into access point mode to reconfigure WiFi, making network changes very simple.
Clear OLED Display: Information is presented clearly on a 128x64 pixel SSD1306 OLED screen.

## Required Components
ESP8266: A development board such as NodeMCU ESP-12E, Wemos D1 Mini, etc.
OLED Screen: A 128x64 pixel I2C SSD1306 OLED display.

### Connection Wires: To connect the components.

### Push Button (Optional): If your ESP8266 board does not have a built-in button on GPIO0 (D3). It is recommended to use the Flash / Boot button if available.

### Wiring
This project uses I2C communication for the OLED screen and a GPIO pin for the button.
| Component | ESP8266 Pin | Description |
| :---         | :---       | :---  |
| OLED SDA   | D2 (GPIO4)    | I2C Data Line    |
| OLED SCL     | D1 (GPIO5)      | I2C Clock Line     |
| OLED VCC     | 3V3      | 3.3V Power Supply    |
| OLED GND     | GND      | Ground     |
| Button     | D3 (GPIO0)      | Button for WiFi reconfiguration (wire to GND)    |
| Button     | GND     | Ground    |


(Note: Your ESP8266 board's reset button (often labeled Flash or Boot) is typically connected to GPIO0 and can be used for this functionality.)

## Configuration and Installation
### 1. Install Arduino IDE
If you haven't already, download and install the Arduino IDE from the official website.
### 2. Add ESP8266 Board Support to Arduino IDE
Open the Arduino IDE.
Go to File > Preferences.
In "Additional Boards Manager URLs", add:
http://arduino.esp8266.com/stable/package_esp8266com_index.json
Go to Tools > Board > Boards Manager....
Search for "esp8266" and install the latest version of "esp8266 by ESP8266 Community".
### 3. Install Libraries
You need to install the following libraries via the Arduino IDE Library Manager (Sketch > Include Library > Manage Libraries...):
Adafruit GFX Library (by Adafruit)
Adafruit SSD1306 (by Adafruit)
NTPClient (by Fabrice Weinberg)
ArduinoJson (by Benoit Blanchon) - Version 6 or higher.
WiFiManager (by tzapu)
### 4. Upload the Code
Copy the provided code into the Arduino IDE.
Ensure the correct Board type is selected under Tools > Board (e.g., NodeMCU 1.0 (ESP-12E Module) or Generic ESP8266 Module).
Select the correct Serial Port under Tools > Port.
Click the Upload button (right arrow icon) to compile and upload the code to your ESP8266.

## Usage
### First Configuration (or after resetting parameters)
After uploading, your ESP8266 will boot up. The OLED screen will display "Connexion WiFi...".
If the ESP8266 does not find a saved WiFi network, it will create its own Access Point (AP) named "AutoConnectAP".
On your phone or computer, connect to the WiFi network "AutoConnectAP" (the default password is "password", if set in the code).
Once connected, a captive portal should open automatically. If not, open your web browser and go to 192.168.4.1.
Follow the on-screen instructions to select your home WiFi network and enter the password.
After saving, the ESP8266 will restart and connect to your network. The screen will then display the BTC/ETH/BNB information and the time.

### Changing WiFi Network (without recompiling)
To change the WiFi network your ESP8266 is connected to:
Hold down the button connected to pin D3 (GPIO0) for approximately 3 seconds. (This is often the Flash or Boot button on your NodeMCU/Wemos board).
The OLED screen will indicate that WiFi configuration mode is activated.
The ESP8266 will enter access point mode, and you can reconnect to the "AutoConnectAP" network and access the 192.168.4.1 portal to choose a new network or modify existing settings.
After saving the new settings, the ESP8266 will restart and connect to the new network.

### Troubleshooting
Blank OLED Screen / I2C Error:
Check SDA (D2/GPIO4) and SCL (D1/GPIO5) wiring.
Ensure the I2C address (0x3C or 0x3D) is correct for your screen. The "ESP8266 I2C Scanner Utility" tool can help you find the address.

WiFi Connection Issues:
Verify that the configuration access point name ("AutoConnectAP") and password are correct during the initial setup.
Ensure the WiFi network you are trying to join is 2.4 GHz (the ESP8266 does not support 5 GHz).

Crypto Data Not Displayed / Incorrect Time:
Make sure your ESP8266 is successfully connected to the internet (check the IP address displayed on the OLED or in the serial monitor).
HTTPS requests require client.setInsecure(); if you are not using root certificates. This is convenient for development but not secure for critical use.

Enjoy your ESP8266 crypto display!
