# All-In-One Meter
## What It Is
This device retrieves data from public REST APIs and displays it on 6 VFD tubes. It can also control IoT devices with REST API.

Public REST APIs (Application Programming Interfaces) provide a standardized method for software applications to communicate and exchange data over the Internet. These APIs are publicly accessible and enable the delivery of information from various online services, platforms, and databases. Public REST APIs are based on the REST architectural style, which relies on the HTTP protocol for data transfer and typically returns responses in widely supported formats such as JSON or XML. These APIs allow developers to leverage existing functionality, services, and resources provided by organizations, creating opportunities for integration, innovation, and new application development. 

Integration with public REST APIs opens up a world of possibilities. By utilizing these APIs, the device can obtain real-time data from diverse sources such as weather services, stock markets, and environmental monitoring systems. Additionally, the device is capable of displaying the date and time.
At the core of the meter lies an ESP32 microcontroller renowned for its versatility and connectivity capabilities. This chip provides the necessary processing power and integrates various communication protocols, making it the ideal choice for accessing REST APIs and controlling the VFD tubes simultaneously.
The meter's visual interface is achieved through six vacuum fluorescent display (VFD) tubes. These retro-style tubes offer clear and easily readable information.
The meter's potential extends beyond data retrieval and display—it can also function as a control hub for various IoT devices. By leveraging REST APIs designed for IoT device management, users can remotely monitor and control compatible devices. Whether it involves adjusting the temperature of a smart thermostat, managing lighting systems, or controlling home security, the meter puts the power of IoT control in the hands of the user.
You can find a good choice of public REST APIs here: https://github.com/public-apis/public-apis

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