#include <TB6612_ESP32.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <Adafruit_NeoPixel.h>

#define PIN_NEO_PIXEL  16 
#define NUM_PIXELS     16 

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);

#define AIN1 13 // ESP32 Pin D13 to TB6612FNG Pin AIN1
#define BIN1 12 // ESP32 Pin D12 to TB6612FNG Pin BIN1
#define AIN2 14 // ESP32 Pin D14 to TB6612FNG Pin AIN2
#define BIN2 27 // ESP32 Pin D27 to TB6612FNG Pin BIN2
#define PWMA 26 // ESP32 Pin D26 to TB6612FNG Pin PWMA
#define PWMB 25 // ESP32 Pin D25 to TB6612FNG Pin PWMB
#define STBY 33 // ESP32 Pin D33 to TB6612FNG Pin STBY

const char* ssid = "";
const char* password = "";

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
	body
	{
		display: flex;
		justify-content: center;
	}
	.container
	{
		width: 20%;
		height: 100%;
	}
	.slider
	{
		-webkit-appearance: none;
		width: 90vh;
		height: 25px;
		background: #d3d3d3;
		transform-origin: center left;
		transform: rotate(-90deg) translate(-90vh, 10px);
	}
</style>
<div class = "container">
	<p>Value: <span id = "LeftValue"></span></p>
	<input type = "range" min = "-255" max = "255" value = "0" class = "slider" id = "LeftSlider">
</div>
<div class = "container">
	<p>Value: <span id = "RightValue"></span></p>
	<input type = "range" min = "-255" max = "255" value = "0" class = "slider" id = "RightSlider">
</div>

<script>
let gateway = `ws://${window.location.hostname}/ws`;
let LeftSlider = document.getElementById("LeftSlider");
let LeftOutput = document.getElementById("LeftValue");
let RightSlider = document.getElementById("RightSlider");
let RightOutput = document.getElementById("RightValue");
let websocket;

window.addEventListener('load', onload);

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


String getSensorReadings(){
  readings["s"] = String(slider);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
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
    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings);
  }
}

void move()) {  
  
  motor1.drive(Left);
  motor2.drive(Right);

  if(Left == 0)
  {
    motor1.break();
  }
  if(Right == 0)
  {
    motor2.break();
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
  NeoPixel.begin(); 
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
  if (led == 1) {
    for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {          
      NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 255, 255));
    }
    NeoPixel.show();
  }
  else {
    NeoPixel.clear();
    NeoPixel.show();
  }

  move();
  
  if (fire == 1) {
    digitalWrite(RELAY_PIN, HIGH);
  }
  else {
    digitalWrite(RELAY_PIN, LOW);
  }
  //Serial.println(String(direction) + " " + String(slider) + " " + String(fire) + " " + String(led));
}
