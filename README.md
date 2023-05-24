# All-In-One Meter
## What It Is
A device that can be (almost) everything. Clock, calendar, stock ticker or weather station. The data is retrieved public REST APIs over the Internet and displayed on VFD tubes. REST stands for REpresentational State Transfer, API for Application Programming Interface. This refers to a programming interface that describes the communication between applications (e.g. apps or IoT devices) and web services in networks. You can find a good choice of public REST APIs here: https://github.com/public-apis/public-apis   
An ESP32 handles both the connection via REST APIs and the control of the VFD tubes. The APIs to be accessed are configured in a JSON file. This provides the greatest possible flexibility. The device is controlled by a rotary encoder. This allows to select a specific web service (or its REST API) as well as the individual data points returned by it.   
There are still large stocks of the soviet VFD tubes type IV-11, so they can be purchased cheaply via Ebay or online stores. Under the tubes are WS2812 compatible RGB LEDs, which provide a nice color accent.

## Configuration
The device's configuration can be modified by editing the 'data/settings.json' file, which allows customization of the following parameters:
- WLAN SSID and password
- Hostname for remote access
- NTP server address
- Time zone
- Date and time format using standard date and time format specifiers
- LED colors while connecting to WLAN or waiting for server response
- Array with API queries:
  - Alias: String displayed on the tubes when a query is selected
  - Refresh: Interval at which data is polled in seconds
  - URL: API URL
  - GET: Array with the values of interest
- Array with APIs of devices to control
  - Alias: String displayed on the tubes when a query is selected
  - URL: URL of device to control
  - Value: Value to send to the device after it has been selected
  - Step, Min, Max: How the value changes, when the encoder is turned

The settings.json file can be easily uploaded to the SPIFFs filesystem of the ESP32 using the provided batch file "spiffs.bat".

## Usage
### Query APIs
To select a query, press and hold the rotary encoder while turning it until the desired query's alias appears, then release it. Rotate the rotary encoder to choose a data item returned by the query.
### Control APIs
To modify the LED color or brightness, or control an IoT device, quickly press the rotary encoder twice within one second. Press and hold the encoder while turning it until the alias of the desired device to control is displayed on the tubes, then release it. Rotating the encoder sends the value to the device.