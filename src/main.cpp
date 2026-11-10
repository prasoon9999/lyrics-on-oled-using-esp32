#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace {
constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_RESET = -1;
constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
String artist;
String title;
String lyrics;
size_t lyricPosition = 0;
unsigned long lastDisplayUpdate = 0;

String urlEncode(const String& value) {
  const char hex[] = "0123456789ABCDEF";
  String encoded;
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (isalnum(character) || character == '-' || character == '_' ||
        character == '.' || character == '~') {
      encoded += static_cast<char>(character);
    } else {
      encoded += '%';
      encoded += hex[character >> 4];
      encoded += hex[character & 0x0F];
    }
  }
  return encoded;
}

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>YouTube Music OLED lyrics</title>
<style>
body{font-family:system-ui;max-width:520px;margin:2rem auto;padding:0 1rem}
input,button{box-sizing:border-box;width:100%;padding:.8rem;margin:.35rem 0;font-size:1rem}
button{cursor:pointer;background:#111;color:#fff;border:0;border-radius:.35rem}
p{color:#555}
</style></head>
<body>
<h1>YouTube Music lyrics</h1>
<p>Keep YouTube Music playing, then enter the artist and song title.</p>
<form action="/set" method="get">
<input name="artist" placeholder="Artist" required>
<input name="title" placeholder="Song title" required>
<button>Show lyrics on OLED</button>
</form>
</body></html>
)HTML";

void showMessage(const String& first, const String& second = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(first);
  if (second.length()) display.println(second);
  display.display();
}

String htmlEscape(const String& value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  return escaped;
}

bool fetchLyrics() {
  showMessage("Fetching lyrics...", title);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://lrclib.net/api/get?artist_name=" + urlEncode(artist) +
               "&track_name=" + urlEncode(title);
  if (!http.begin(client, url)) {
    showMessage("Lyrics connection", "failed");
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    showMessage("Lyrics not found", String(status));
    return false;
  }

  DynamicJsonDocument document(32768);
  const DeserializationError error = deserializeJson(document, http.getString());
  http.end();
  if (error) {
    showMessage("Invalid lyrics", "response");
    return false;
  }

  const char* synced = document["syncedLyrics"];
  const char* plain = document["plainLyrics"];
  lyrics = synced ? String(synced) : (plain ? String(plain) : "");
  lyricPosition = 0;
  if (lyrics.isEmpty()) {
    showMessage("No lyrics found");
    return false;
  }
  return true;
}

void handleHome() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleSet() {
  if (!server.hasArg("artist") || !server.hasArg("title")) {
    server.send(400, "text/plain", "Artist and title are required");
    return;
  }
  artist = server.arg("artist");
  title = server.arg("title");
  if (!fetchLyrics()) {
    server.send(404, "text/html", "<p>Lyrics were not found. <a href='/'>Try again</a>.</p>");
    return;
  }
  server.send(200, "text/html",
              "<p>Showing <b>" + htmlEscape(title) + "</b> by " +
                  htmlEscape(artist) + " on the OLED.</p><p><a href='/'>Change song</a></p>");
}

void drawNextLyricPage() {
  if (lyrics.isEmpty()) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(title.substring(0, 21));

  String line;
  uint8_t row = 2;
  while (lyricPosition < lyrics.length() && row < 8) {
    const char character = lyrics[lyricPosition++];
    if (character == '\n' || line.length() >= 20) {
      if (line.startsWith("[")) {
        const int closingBracket = line.indexOf(']');
        if (closingBracket >= 0) line = line.substring(closingBracket + 1);
      }
      if (line.length()) {
        display.setCursor(0, row * 8);
        display.println(line);
        ++row;
      }
      line = "";
    } else if (character != '\r') {
      line += character;
    }
  }
  if (line.length() && row < 8) {
    display.setCursor(0, row * 8);
    display.println(line);
  }
  display.display();
}
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true) delay(1000);
  }
  showMessage("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Open http://");
  Serial.println(WiFi.localIP());
  showMessage("Open this address:", WiFi.localIP().toString());

  server.on("/", HTTP_GET, handleHome);
  server.on("/set", HTTP_GET, handleSet);
  server.begin();
}

void loop() {
  server.handleClient();
  if (!lyrics.isEmpty() && millis() - lastDisplayUpdate >= 5000) {
    lastDisplayUpdate = millis();
    if (lyricPosition >= lyrics.length()) lyricPosition = 0;
    drawNextLyricPage();
  }
}
