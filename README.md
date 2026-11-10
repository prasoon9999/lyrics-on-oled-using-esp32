# YouTube Music OLED Lyrics

This is a new, separate ESP32 project. It does not modify the Smart Weather
Station project.

It displays lyrics for a song currently playing in YouTube Music on an
SSD1306 I2C 128x64 OLED. Because the ESP32 has no audio hardware or reliable
way to identify YouTube Music playback automatically, enter the artist and
song title through the ESP32's local web page. Lyrics are fetched from
[LRCLIB](https://lrclib.net/).

## Wiring

| OLED | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL/SCK | GPIO 22 |

## Setup

1. Open this folder in VS Code with PlatformIO.
2. Edit `src/main.cpp` and replace `YOUR_WIFI_NAME` and
   `YOUR_WIFI_PASSWORD`.
3. Build and upload to the ESP32.
4. Open the serial monitor at 115200 baud.
5. Visit the IP address printed by the ESP32 from a device on the same Wi-Fi.
6. Enter the artist and title of the YouTube Music song.

The OLED cycles through lyric pages every five seconds. LRCLIB may not have
lyrics for every song, and copyrighted lyrics are not stored in this project.
