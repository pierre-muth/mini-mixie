/*
	Mini-nixie clock based on ESP32
	pierremuth.wordpress.com
		
    Simple web configuration portal:
    https://github.com/copercini/esp32-iot-examples/blob/master/WiFi_portal/WiFi_portal.ino
    by Evandro Luis Copercini - 2017
    Public Domain


*/
#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <SPI.h>
#include <Preferences.h>
#include "htmlStuff.h"

#define AP_SSID  "Simon's mini-nixie"       //AP hostname
#define BL 32
#define LE 33
#define HV 27
#define ButtonBoot 0

// Proto
void wifiOnConnect();
void wifiOnDisconnect();
void getTimeAndDisplay();
void replyToWebServer();
void refreshNixies();
void resetParameterSequence();
void sendHTML(WiFiClient client);
void displayIP();

// constants
const char* ntpServer = "pool.ntp.org";

static const uint32_t numbers[] = 
    {0b1000000000, 0b0000000001, 0b0000000010, 0b0000000100, 0b0000001000,
    0b0000010000, 0b0000100000, 0b0001000000, 0b0010000000, 0b0100000000, 0b0000000000};
static const uint32_t scan[7][6] = {{10, 10, 10, 10, 10, 1}, {10, 10, 10, 10, 1, 10}, 
    {10, 10, 10, 1, 10, 10}, {10, 10, 1, 10, 10, 10}, {10, 1, 10, 10, 10, 10}, 
    {1, 10, 10, 10, 10, 10}, {1, 1, 1, 1, 1, 1}};
static const uint32_t number_scan[18] = {1, 0, 2, 9, 3, 8, 4, 5, 7, 6, 7, 5, 4, 8, 3, 9, 2, 0};
static const uint32_t number_localIP[4][6] = {{1, 9, 2, 10, 10, 10}, 
                                    {1, 6, 8, 10, 10, 10}, 
                                    {10, 10, 4, 10, 10, 10}, 
                                    {10, 10, 1, 10, 10, 10}};

static const int spiClk = 100000; 

// global vars
WiFiServer server(80);
Preferences preferences;
static volatile bool wifi_connected = false;
String wifiSSID;
String wifiPassword;
long  gmtOffset_sec = 36000;
int   daylightOffset_sec = 0;
int   brightness_normal = 0;
int   brightness_low = 128;
int   saving_start = 23;
int   saving_end = 6;
int   loopN = 0;

IPAddress Ip(192, 168, 4, 1);
IPAddress NMask(255, 255, 255, 0);
SPIClass * hspi = NULL; //uninitalised pointers to SPI object
struct tm timeinfo;
uint32_t HVhigh = 0x00000000;
uint32_t HVlow = 0x00000000;
uint8_t nixies[] = {0,0,0,0,0,0};
uint8_t dots[] = {0,0,0,0};

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {

        case SYSTEM_EVENT_AP_START:
            //can set ap hostname here
            WiFi.softAPsetHostname(AP_SSID);
            //config AP IP  
            WiFi.softAPConfig(Ip, Ip, NMask);
            //enable ap ipv6 here
            WiFi.softAPenableIpV6();
            break;

        case SYSTEM_EVENT_STA_START:
            //set sta hostname here
            WiFi.setHostname(AP_SSID);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            //enable sta ipv6 here
            WiFi.enableIpV6();
            break;
        case SYSTEM_EVENT_AP_STA_GOT_IP6:
            //both interfaces get the same event
            Serial.print("STA IPv6: ");
            Serial.println(WiFi.localIPv6());
            Serial.print("AP IPv6: ");
            Serial.println(WiFi.softAPIPv6());
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            wifiOnConnect();
            wifi_connected = true;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_connected = false;
            wifiOnDisconnect();
            break;
        default:
            break;
    }
}

String urlDecode(const String& text) {
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len) {
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len)) {
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);
			decodedChar = strtol(temp, NULL, 16);
		} else {
			if (encodedChar == '+') {
				decodedChar = ' ';
			} else {
				decodedChar = encodedChar;  // normal ascii char
			}
		}
		decoded += decodedChar;
	}
	return decoded;
}


void setup() {
    Serial.begin(115200);

    // GPIO init
    pinMode(BL, OUTPUT);
    pinMode(LE, OUTPUT);
    pinMode(HV, OUTPUT);

    digitalWrite(LE, HIGH);
    digitalWrite(BL, LOW);
    digitalWrite(HV, HIGH);

    // SPI init
    hspi = new SPIClass(HSPI);
    hspi->begin(25, 12, 26, 14); //SCLK, MISO, MOSI, SS

    // digits init
    HVlow = numbers[0];
    HVlow |= numbers[0] << 10;
    HVlow |= numbers[0] << 20;
    HVhigh = numbers[0];
    HVhigh |= numbers[0] << 10;
    HVhigh |= numbers[0] << 20;
    HVlow = ~HVlow;
    HVhigh = ~HVhigh;
    hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
    hspi->transfer32(HVhigh);
    hspi->transfer32(HVlow);
    hspi->endTransaction();

    // copy to latches
    digitalWrite(LE, LOW);
    delay(1);
    digitalWrite(LE, HIGH);

    // enable high voltage
    digitalWrite(HV, LOW);

    //Wifi
    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(AP_SSID);
    delay(1000);
    Serial.println("AP Started");
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IPv4: ");
    Serial.println(WiFi.softAPIP());

    preferences.begin("wifi", false);
    wifiSSID = preferences.getString("ssid", "none");           //NVS key ssid
    wifiPassword = preferences.getString("password", "");   //NVS key password
    preferences.end();
    preferences.begin("gmt", false);
    gmtOffset_sec = preferences.getLong("gmtOffset_sec", 0);           //GMT offset in seconds
    daylightOffset_sec = preferences.getInt("daylOffset_sec", 0);   //Daylight saving offset in seconds
    preferences.end();
    preferences.begin("settings", false);
    brightness_normal = preferences.getInt("normal", 0);
    brightness_low = preferences.getInt("low", 0);
    saving_start = preferences.getInt("start", 23);
    saving_end = preferences.getInt("end", 6);
    preferences.end();
    Serial.println("Stored preferences: ");
    Serial.println(wifiSSID);
    Serial.println(wifiPassword);
    Serial.println(gmtOffset_sec);
    Serial.println(daylightOffset_sec);
    Serial.println(brightness_normal);
    Serial.println(brightness_low);
    Serial.println(saving_start);
    Serial.println(saving_end);
   
    // PWM, nixies brightness
    ledcSetup(0, 100, 8);
    ledcAttachPin(BL, 0);
    ledcWrite(0, brightness_normal);
    
    // start wifi
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    // start web server
    server.begin();


}

void loop() {
    loopN++;
    
    if (wifi_connected){
        getTimeAndDisplay();
        delay(500);
        replyToWebServer();
    } else if (loopN > 60) {    // still no wifi, display local wifi Access point IP address
        for (int n=0; n<4; n++) {
            for (int j=0; j<6; j++) nixies[j] = number_localIP[n][j];
            dots[3] = 0;
            dots[1] = 0;
            dots[0] = 0;
            dots[2] = 0;
            refreshNixies();
            for (int i=0; i<7; i++) {
                replyToWebServer();
                delay(100);
            }
            for (int j=0; j<6; j++) nixies[j] = 10;
            dots[3] = 0;
            dots[1] = 0;
            dots[0] = 0;
            dots[2] = n < 3;
            refreshNixies();
            for (int i=0; i<7; i++) {
                replyToWebServer();
                delay(100);
            }
        }
    } else {  // waiting if wifi will connect
        for (int i=0; i<6; i++) nixies[i] = 10;
        dots[3] = (loopN%4 == 3);
        dots[1] = (loopN%4 == 2);
        dots[0] = (loopN%4 == 1);
        dots[2] = (loopN%4 == 0);
        refreshNixies();
        delay(50);
        replyToWebServer();
    }
    

    // holding BOOT button (resets the preferences)
    if (digitalRead(ButtonBoot) == LOW) {
        Serial.println("Button boot LOW...");
        resetParameterSequence();
    }
}

void resetParameterSequence(){
    dots[0] = 0; dots[1] = 0; dots[2] = 0; dots[3] = 0;
    for (int i=0; i<6; i++) nixies[i] = 10;
    refreshNixies();

    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[0][i];
        refreshNixies();
        delay(300);
    } else {
        displayIP();
        return;
    }
    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[1][i];
        refreshNixies();
        delay(300);
    } else {
        displayIP();
        return;
    }
    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[2][i];
        refreshNixies();
        delay(300);
    } else {
        displayIP();
        return;
    }
    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[3][i];
        refreshNixies();
        delay(300);
    } 
    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[4][i];
        refreshNixies();
        delay(300);
    }
    if (digitalRead(ButtonBoot) == LOW) {
        for (int i=0; i<6; i++) nixies[i] = scan[5][i];
        refreshNixies();
        delay(300);
    }
    if (digitalRead(ButtonBoot) == LOW) {
        Serial.println("Clearing wifi preferences");
        preferences.clear();
        preferences.end();

        preferences.begin("wifi", false);
        preferences.putString("ssid", "");          //wifi parameters
        preferences.putString("password", "");
        delay(300);
        preferences.end();

        for (int i=0; i<6; i++) nixies[i] = scan[6][i];
        refreshNixies();
        delay(1000);
        ESP.restart();
    }
}

void displayIP() {
    for (int n=0; n<4; n++) {    
        nixies[0] = (WiFi.localIP()[n]/100)%10;
        nixies[1] = (WiFi.localIP()[n]/10)%10;
        nixies[2] = WiFi.localIP()[n]%10;
        nixies[3] = 10;
        nixies[4] = 10;
        nixies[5] = 10;

        dots[3] = 0;
        dots[1] = 0;
        dots[0] = 0;
        dots[2] = 0;
        refreshNixies();
        delay(700);
        for (int j=0; j<6; j++) nixies[j] = 10;
        dots[3] = 0;
        dots[1] = 0;
        dots[0] = 0;
        dots[2] = n < 3;
        refreshNixies();
        delay(700);
    }
}

//when wifi connects
void wifiOnConnect() {
    // wifi info
    Serial.println("STA Connected");
    Serial.print("STA SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("STA IPv4: ");
    Serial.println(WiFi.localIP());
    Serial.print("STA IPv6: ");
    Serial.println(WiFi.localIPv6());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());

    Serial.println("Closing Wifi Acsess point.");
    WiFi.mode(WIFI_MODE_STA);     //close AP 
  
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

//when wifi disconnects
void wifiOnDisconnect() {
  Serial.println("STA Disconnected");
  delay(1000);
  Serial.println("Opening Wifi Acsess point.");
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
}

// get time with NTP and actualize nixies
void getTimeAndDisplay() {
    // cathode rotating
    if (timeinfo.tm_sec == 0 && timeinfo.tm_min%2 == 0) {
        for(int i=0; i<50; i++){
            delay(50);

            nixies[5] = number_scan[(i+1)%18];  
            nixies[4] = number_scan[(i+2)%18];
            nixies[3] = number_scan[(i+3)%18];
            nixies[2] = number_scan[(i+4)%18];
            nixies[1] = number_scan[(i+5)%18];
            nixies[0] = number_scan[(i+6)%18];

            dots[3] = (i%4 == 3);
            dots[2] = (i%4 == 2);
            dots[1] = (i%4 == 1);
            dots[0] = (i%4 == 0);
            
            // refresh the tubes
            refreshNixies();
        }
    }

    // get time
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        for (int i=0; i<6; i++) nixies[i] = 10;
        dots[3] = (loopN%4 == 3);
        dots[2] = (loopN%4 == 1);
        dots[1] = (loopN%4 == 2);
        dots[0] = (loopN%4 == 0);
        refreshNixies();
        return;
    }

    // saving hours
    if (saving_start < saving_end) {
        if (timeinfo.tm_hour > saving_start && timeinfo.tm_hour < saving_end) {
            ledcWrite(0, brightness_low);
        } else {
            ledcWrite(0, brightness_normal);
        }
    } else {
        if (timeinfo.tm_hour > saving_start || timeinfo.tm_hour < saving_end) {
            ledcWrite(0, brightness_low);
        } else {
            ledcWrite(0, brightness_normal);
        }
    }

    Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print(", RSSI: ");
    Serial.println(WiFi.RSSI());

    // generate time numbers
    nixies[5] = timeinfo.tm_sec % 10;
    nixies[4] = timeinfo.tm_sec / 10;
    nixies[3] = timeinfo.tm_min % 10;
    nixies[2] = timeinfo.tm_min / 10;
    nixies[1] = timeinfo.tm_hour % 10;
    nixies[0] = timeinfo.tm_hour / 10;

    // colomn separator blinking
    dots[0] = timeinfo.tm_sec % 3 == 0;
    dots[1] = timeinfo.tm_sec % 3 == 0;
    dots[2] = timeinfo.tm_sec % 3 == 0;
    dots[3] = timeinfo.tm_sec % 3 == 0;

    // refresh the tubes
    refreshNixies();
    
}

void refreshNixies() {

    // generate time numbers
    HVlow = numbers[ nixies[5] ];
    HVlow |= numbers[ nixies[4] ] << 10;
    HVlow |= numbers[ nixies[3] ] << 20;
    HVhigh = numbers[ nixies[2] ];
    HVhigh |= numbers[ nixies[1] ] << 10;
    HVhigh |= numbers[ nixies[0] ] << 20;

    // colomn dot separators
    if (dots[3]) HVlow |= 0x80000000;
    if (dots[2]) HVlow |= 0x40000000;
    if (dots[1]) HVhigh |= 0x80000000;
    if (dots[0]) HVhigh |= 0x40000000;

    // inverting due to level translators
    HVlow = ~HVlow;
    HVhigh = ~HVhigh;
    
    // send on SPI
    hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
    hspi->transfer32(HVhigh);
    hspi->transfer32(HVlow);
    hspi->endTransaction();

    // copy to latches
    digitalWrite(LE, LOW);
    delay(1);
    digitalWrite(LE, HIGH);

}

void sendHTML(WiFiClient client) {
    for (int i=0; i<htmlLength; i++)
        client.println(htmlLines[i]);
}

void replyToWebServer() {
    WiFiClient client = server.available();   // listen for incoming clients

    if (client) {                             // if you get a client,
        Serial.println("New client");           // print a message out the serial port
        for (int i=0; i<6; i++) nixies[i] = 10;
        for (int i=0; i<4; i++) dots[i] = 1;
        refreshNixies();
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected()) {            // loop while the client's connected
            if (client.available()) {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                Serial.write(c);                    // print it out the serial monitor
                if (c == '\n') {                    // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        sendHTML(client);                        
                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    } else {    // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                } else if (c != '\r') {  // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                    continue;
                }

                if (currentLine.startsWith("GET /a?ssid=") ) {
                    //Expecting something like:
                    //GET /a?ssid=blahhhh&pass=poooo
                    Serial.println("");
                    Serial.println("new WiFi credentials");
                    // End opened namespace
                    preferences.end();

                    String qsid;
                    qsid = urlDecode(currentLine.substring(12, currentLine.indexOf('&'))); //parse ssid
                    Serial.print(qsid);
                    Serial.print(", ");
                    String qpass;
                    qpass = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); //parse password
                    Serial.println(qpass);

                    preferences.begin("wifi", false); // Note: Namespace name is limited to 15 chars
                    Serial.println("Writing new ssid");
                    preferences.putString("ssid", qsid);

                    Serial.println("Writing new pass");
                    preferences.putString("password", qpass);
                    delay(300);
                    preferences.end();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();

                    // the content of the HTTP response follows the header:
                    client.print("<h1>WiFi credentials Updated, restarting in 2 seconds...</h1>");
                    client.println();

                    delay(1000);
                    ESP.restart();
                }

                if (currentLine.startsWith("GET /a?gmtoff=") ) {
                    //Expecting something like:
                    //GET /a?gmtoff=3600&daylightoff=3600
                    Serial.println("");
                    Serial.println("New GMT offsets: ");
                    // End opened namespace
                    preferences.end();

                    String gmtOffset;
                    gmtOffset = urlDecode(currentLine.substring(14, currentLine.indexOf('&'))); //parse gmt offset
                    Serial.print(gmtOffset);
                    Serial.print(", ");
                    String daylightOffset;
                    daylightOffset = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); //parse daylight offset
                    Serial.println(daylightOffset);

                    gmtOffset_sec = strtol(gmtOffset.c_str(), NULL, 10);
                    daylightOffset_sec = daylightOffset.toInt();

                    preferences.begin("gmt", false);
                    preferences.putLong("gmtOffset_sec", gmtOffset_sec);           //GMT offset in seconds
                    preferences.putInt("daylOffset_sec", daylightOffset_sec);   //Daylight saving offset in seconds
                    delay(300);
                    preferences.end();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();

                    // the content of the HTTP response follows the header:
                    client.print("<h1>Time zone and daylight saving updated. </h1>");
                    client.println();

                    delay(1000);
                    ESP.restart();
                }

                if (currentLine.startsWith("GET /a?normal=") ) {
                    //Expecting something like:
                    //GET /a?bright=128
                    Serial.println("");
                    Serial.println("New brightness: ");
                    // End opened namespace
                    preferences.end();

                    String bright_norm;
                    bright_norm = urlDecode(currentLine.substring(14, currentLine.indexOf('&'))); 
                    Serial.print(bright_norm);
                    Serial.print(", ");

                    String bright_lo;
                    bright_lo = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); 
                    Serial.println(bright_lo);

                    brightness_normal = bright_norm.toInt();
                    brightness_low = bright_lo.toInt();

                    if (brightness_normal < 0) brightness_normal = 0;
                    if (brightness_normal > 255) brightness_normal = 255;
                    brightness_normal = 255-brightness_normal;
                    if (brightness_low < 0) brightness_low = 0;
                    if (brightness_low > 255) brightness_low = 255;
                    brightness_low = 255-brightness_low;

                    preferences.begin("settings", false);
                    preferences.putInt("normal", brightness_normal);   // Nixies brightness (PWM)
                    preferences.putInt("low", brightness_low);
                    delay(300);
                    preferences.end();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();

                    // the content of the HTTP response follows the header:
                    client.print("<h1>Brightness updated. </h1>");
                    client.println();
                    client.stop();
                    delay(1000);
                    ESP.restart();
                }

                if (currentLine.startsWith("GET /a?start=") ) {
                    //Expecting something like:
                    //GET /a?start=21
                    Serial.println("");
                    Serial.println("New saving hours: ");
                    // End opened namespace
                    preferences.end();

                    String start_hour;
                    start_hour = urlDecode(currentLine.substring(13, currentLine.indexOf('&'))); 
                    Serial.print(start_hour);
                    Serial.print(", ");

                    String end_hour;
                    end_hour = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); 
                    Serial.println(end_hour);

                    saving_start = start_hour.toInt();
                    saving_end = end_hour.toInt();

                    if (saving_start < 0) saving_start = 0;
                    if (saving_start > 23) saving_start = 23;
                    if (saving_end < 0) saving_end = 0;
                    if (saving_end > 23) saving_end = 23;

                    preferences.begin("settings", false);
                    preferences.putInt("start", saving_start);   
                    preferences.putInt("end", saving_end);
                    delay(300);
                    preferences.end();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();

                    // the content of the HTTP response follows the header:
                    client.print("<h1>Saving time updated. </h1>");
                    client.println();
                    client.stop();
                    delay(1000);
                    ESP.restart();
                }
            }
        }
        // close the connection:
        client.stop();
        Serial.println("client disconnected");
    }
}