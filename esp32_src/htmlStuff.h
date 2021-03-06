#include <Arduino.h>

const int htmlLength = 69;
const String htmlLines[htmlLength] = {
  "HTTP/1.1 200 OK",
  "Content-type:text/html",
  "Connection: close",
  "",
  "<!DOCTYPE html><html>",
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
  "<link rel=\"icon\" href=\"data:,\">",
  "<style>",
  "html{font-family:Helvetica;display:inline-block;margin:0 auto;text-align:center;background-color:#eee;}",
  "div{padding:.3em;}",
  "form{margin:0 auto;width:250px;padding:.5em;border:2px solid #fff;border-radius:1em;}",
  "select{width:150px;}",
  "label{display:inline-block;width:80px;text-align:right;}",
  "input{width:150px;box-sizing:border-box;border:2px solid #f90;}",
  "button{width: 234;padding:.5em;font-family:Arial;font-size:15px;font-weight:bold;border-radius:1em;}",
  "</style>",
  "</head> <body> <h1>Simon's Mini-Nixie Clock</h1>",
  "Press the BOOT button to display IP address. <br>",
  "Hold the BOOT button to reset all the settings. <br>",
  "Press the RESET button in case of wrong behavior. <br><br>",
  "<form action='a' method='get'>",
  "<strong>WiFi parameters:</strong>",
  "<div>",
  "<label>SSID: </label>",
  "<input type='text' name='ssid'/>",
  "<label>password: </label>",
  "<input type='text' name='password'/>",
  "</div>",
  "<div>",
  "<button type='submit'>Set and connect</button>",
  "</div>",
  "</form>",
  "<form action='a' method='get'>",
  "<strong>GMT:</strong> (in seconds)",
  "<div>",
  "<label>offset: </label>",
  "<input type='text' name='gmtoff'/>",
  "<label>daylight: </label>",
  "<input type='text' name='daylOff'/>",
  "</div>",
  "<div>",
  "<button type='submit'>Set</button>",
  "</div>",
  "</form>",
  "<form action='a' method='get'>",
  "<strong>Brightness:</strong> (0 - 255)",
  "<div>",
  "<label>Normal: </label>",
  "<input type='text' name='normal'/>",
  "<label>Saving: </label>",
  "<input type='text' name='low'/>",
  "</div>",
  "<div>",
  "<button type='submit'>Set</button>",
  "</div>",
  "</form>",
  "<form action='a' method='get'>",
  "<strong>Saving Hours:</strong>",
  "<div>",
  "<label>Start: </label>",
  "<input type='text' name='start'/>",
  "<label>End: </label>",
  "<input type='text' name='end'/>",
  "</div>",
  "<div>",
  "<button type='submit'>Set</button>",
  "</div>",
  "</form>",
  "</body></html>"
};
