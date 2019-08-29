# gree_thermostat
ESP8266 based IR remote control with thermostat for GREE/Tadiran split AC units

This project allows MQTT control for Tadiran A-Series (GREE) split AC units by using an infrared LED and Si7021 sensor. All these can be bought for little money on Aliexpress.

It measures the temperature and calculates an average for 16 (AVERAGE_LENGTH) samples every 5 seconds. Every minute, it will calculate if the set temperature differs from the average and adjust the AC temperature by the difference. This way it compensates for the AC thermostat being not too reliable.
TODO: If this isn't good enough, add an I term to the regulator.

Buy a cheap ESP8266 board like the Wemos D1 mini and connect the IR transmitter module (take one with built-in transistor, not the naked LED) to D0 and connect the Si7021 module to D2 (SDA) and D1 (SCL).

Then modify the source by replacing YOUR SSID, YOUR PASSWORD and YOUR MQTT SERVER IP ADDRESS to match your environment and program the ESP8266. 
If you want to change the topic, do a search/replace for Tadiran_1 and replace it with something you prefer. If you need more than one of these, you can just replace it with Tadiran_2 or so.

Below is the Home Assistant integration:

```yml
sensor:
  - platform: mqtt
    name: "LivingroomTemperature"
    state_topic: "Tadiran_1/RoomTemp"
  - platform: mqtt
    name: "LivingroomHumidity"
    state_topic: "Tadiran_1/RoomHumidity"

climate:
  - platform: mqtt
    name: Livingroom
    min_temp: 16
    max_temp: 30
    temp_step: 1
    modes:
      - "off"
      - "cool"
      - "dry"
      - "fan_only"
      - "heat"
    swing_modes:
      - LastPos
      - Auto
      - Up
      - MiddleUp
      - Middle
      - MiddleDown
      - Down
      - DownAuto
      - MiddleAuto
      - UpAuto
    fan_modes:
      - "high"
      - "medium"
      - "low"
      - "auto"
    payload_on: 1
    payload_off: 0
    power_command_topic: "Tadiran_1/SetOn"
    current_temperature_topic: "Tadiran_1/RoomTemp"
    mode_command_topic: "Tadiran_1/SetMode"
    mode_state_template: >-
      {% set modes = { '0':'auto', '1':'cool',  '2':'dry', '3':'fan_only', '4': 'heat'} %}
      {{ modes[value] if value in modes.keys() else 'off' }}
    temperature_command_topic: "Tadiran_1/SetTemp"
    fan_mode_command_topic: "Tadiran_1/SetFan"
    swing_mode_command_topic: "Tadiran_1/SetSwing"
    precision: 1.0
   ```