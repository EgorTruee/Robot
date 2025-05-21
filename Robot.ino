#include <TB6612_ESP32.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#define AIN1 13 // ESP32 Pin D13 to TB6612FNG Pin AIN1
#define BIN1 12 // ESP32 Pin D12 to TB6612FNG Pin BIN1
#define AIN2 14 // ESP32 Pin D14 to TB6612FNG Pin AIN2
#define BIN2 27 // ESP32 Pin D27 to TB6612FNG Pin BIN2
#define PWMA 26 // ESP32 Pin D26 to TB6612FNG Pin PWMA
#define PWMB 25 // ESP32 Pin D25 to TB6612FNG Pin PWMB
#define STBY 33 // ESP32 Pin D33 to TB6612FNG Pin STBY

const char* ssid = "Password";
const char* password = "1234567891";

#define RELAY_PIN 15 // pin G15

int Left = 0;
int Right = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

JSONVar readings;

// these constants are used to allow you to make your motor configuration
// line up with function names like forward.  Value can be 1 or -1
const int offsetA = 1;
const int offsetB = 1;

Motor motor1 = Motor(AIN1, AIN2, PWMA, offsetA, STBY, 5000, 8, 1);
Motor motor2 = Motor(BIN1, BIN2, PWMB, offsetB, STBY, 5000, 8, 2);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
			touch-action: non
        }
        .slider-container {
			margin-top: 150px;
            margin-bottom: 150px; /* Увеличенное расстояние между слайдерами */
        }
        .slider {
            width: 100%;
            height: 10px;
            border-radius: 5px;
            background: #ddd;
            outline: none;
            -webkit-appearance: none;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4CAF50;
            cursor: pointer;
        }
        .slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4CAF50;
            cursor: pointer;
        }
        .value-display {
            margin-top: 5px;
            text-align: center;
            font-size: 14px;
            color: #666;
        }
		.resetButton {
			background-color: red;
			width: 100px;
			height: 100px;
		}
		.buttonConteiner {
			display: flex;
			justify-content: center;
			align-items: center;
		}
</style>
<div class = "slider-container">
	<p>Left Motor Value: <span id = "LeftValue"></span></p>
	<input type = "range" min = "-255" max = "255" value = "0" class = "slider" id = "LeftSlider">
</div>
<div class = "slider-container">
	<p>Right Motor Value: <span id = "RightValue"></span></p>
	<input type = "range" min = "-255" max = "255" value = "0" class = "slider" id = "RightSlider">
</div>
<div class = "buttonConteiner">
	<button class = "resetButton" onclick = "ResetEvent()" id = "resetButton"></button>
</div>
<script>
let gateway = `ws://${window.location.hostname}/ws`;
let LeftSlider = document.getElementById("LeftSlider");
let LeftOutput = document.getElementById("LeftValue");
let RightSlider = document.getElementById("RightSlider");
let RightOutput = document.getElementById("RightValue");
let websocket;

window.addEventListener('load', onload);

function ResetEvent() {
	LeftSlider.value = 0;
	LeftOutput.innerHTML = 0;
	
	RightSlider.value = 0;
	RightOutput.innerHTML = 0;
	
	websocket.send(JSON.stringify({Right : 0, Left : 0}));
}
function onload(event) {
    initWebSocket();
    init();
}

function init() {

	LeftOutput.innerHTML = LeftSlider.value;
	LeftSlider.oninput = function() {
		LeftOutput.innerHTML = this.value;
		websocket.send(JSON.stringify({Left : this.value}));
	}
	RightOutput.innerHTML = RightSlider.value;
	RightSlider.oninput = function() {
		RightOutput.innerHTML = this.value;
		websocket.send(JSON.stringify({Right : this.value}));
	}
}

function toggleBg(btn, state) {
    if (state) {
        btn.style.background = '#ff0000';
    }
    else {
        btn.style.background = '#ffffff';
    }
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function getReadings(){
    websocket.send("getReadings");
}

function onMessage(event) {
    websocket.send("getReadings");
}</script>
)rawliteral";

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    switch(WiFi.status())
    {
      case WL_CONNECTED:
    }

    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    JSONVar myObject = JSON.parse((const char*)data);
    if (myObject.hasOwnProperty("Left")) {
      Left = (int)myObject["Left"];
    }
    if (myObject.hasOwnProperty("Right"))
    {
      Right = (int)myObject["Right"];
    }
  }
}

void move() {  
  
  motor1.drive(Left);
  motor2.drive(Right);

  if(Left == 0)
  {
    motor1.brake();
  }
  if(Right == 0)
  {
    motor2.brake();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);  

  initWiFi();
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
}

void loop()
{
  move();
  //Serial.println(String(direction) + " " + String(slider) + " " + String(fire) + " " + String(led));
}
