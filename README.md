# MQTT ESP32 Mailbox
Inspired by https://learn.adafruit.com/gmailbox/, triggered my MQTT messages. "up" moves the flag up, "down" moves the flag down.
This is a mash-up of https://github.com/espressif/esp-idf/tree/6330b3345e87eb4401e7be7c8b6fea2870c35d9f/examples/protocols/mqtt/tcp and https://github.com/espressif/esp-idf/tree/6330b3345e87eb4401e7be7c8b6fea2870c35d9f/examples/peripherals/mcpwm/mcpwm_servo_control.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu. See "Establishing Wi-Fi or Ethernet Connection" section in [examples/protocols/README.md](../../README.md) for more details.
* When using Make build system, set `Default serial port` under `Serial flasher config`.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
