# LedController
Reactive Led Strip controlled over WiFi. Components used to create this project: NodeMCU, led strip: WS2812B, microphone.

Microphone was used because one of the modes let's led to blink in music rhytm, however it needs further modifications.

In the code you can set your own WiFi ssid and password. Default IP for Led is set to 192.168.1.110 but it can also be changed in the code. 

Repository includes html page to control the led from localhost page.
NOTE: If you change the default IP of NodeMCU you need to change IP in HTML page for all requests.

Special thanks for https://www.tweaking4all.com/ where I found a lot of cool effects for WS2812B, most of them are implemented in this project.
You can also find there how to connect led strip to Arduino. It is highly recommended to get familiar with this page:
https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
