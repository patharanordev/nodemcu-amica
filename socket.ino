/*
 * WebSocketServer_LEDcontrol.ino
 *
 *  Created on: 26.11.2015
 *
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <FS.h>

#define USE_SERIAL Serial

int pressureVoltage = 0;      // variable to store the read value
bool isTracking = false;

ESP8266WiFiMulti WiFiMulti;

ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
            break;
        case WStype_TEXT:
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

            if(payload[0] == '^') {
              String pVal = String(analogRead(A0), DEC);
              webSocket.sendTXT(num, pVal);
            }

            break;
    }
}

String formatBytes(size_t bytes){
    if (bytes < 1024){
        return String(bytes)+"B";
    } else if(bytes < (1024 * 1024)){
        return String(bytes/1024.0)+"KB";
    } else if(bytes < (1024 * 1024 * 1024)){
        return String(bytes/1024.0/1024.0)+"MB";
    } else {
        return String(bytes/1024.0/1024.0/1024.0)+"GB";
    }
}

void createLogFile(){
    if(SPIFFS.exists("/log.txt")){
        USE_SERIAL.println("Log file existing");
    } else {
        File file = SPIFFS.open("/log.txt", "w");
        if(file) {
            file.close();
            USE_SERIAL.println("Log file create success");
        }
        else USE_SERIAL.println("Log file create failed");
    }
}

void setup() {
    // NodeMCU AMICA need to use 9600
    USE_SERIAL.begin(9600);

    //USE_SERIAL.setDebugOutput(true);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    // Set analog input
    pinMode(A0, INPUT);

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
    USE_SERIAL.printf("\n----- File system -----");

    // Check file system
    SPIFFS.begin();
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {    
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            USE_SERIAL.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
        }
        USE_SERIAL.printf("\n");
    }

    // Prepare log file
    createLogFile();

    WiFiMulti.addAP(YOUR_WIFINAME, PASSWORD_WIFI);

    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
    }

    USE_SERIAL.printf("\n----- Network Info -----");
    USE_SERIAL.println("Connected using IP:");
    USE_SERIAL.println(WiFi.localIP());

    USE_SERIAL.println("And MAC address:");
    USE_SERIAL.println(WiFi.macAddress());

    // start webSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if(MDNS.begin("esp8266")) {
        USE_SERIAL.println("MDNS responder started");
    }

    // handle index
    server.on("/", []() {
        // send index.html
        String webContent = "<html>";
        webContent += "<head>";
        webContent += "  <script>";
        webContent += "    var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);";
        webContent += "    var timeInterval = null;";
        
        webContent += "    connection.onopen = function () {";
        webContent += "      connection.send('Connect ' + new Date()); ";
        webContent += "    };"; 
        
        webContent += "    connection.onerror = function (error) {";
        webContent += "      console.log('WebSocket Error ', error);";
        webContent += "    };";
        
        webContent += "    connection.onmessage = function (e) {";  
        webContent += "      console.log('Server: ', e.data);";
        webContent += "      document.getElementById('pressureValue').innerHTML = e.data;";
        webContent += "    };";
        
        webContent += "    function startTracking() {";
        webContent += "      timeInterval = setInterval(function(){";
        webContent += "        connection.send('^');";
        webContent += "      }, 1000);";
        webContent += "    }";
        
        webContent += "    function stopTracking() {";
        webContent += "      if(timeInterval!=null) ";
        webContent += "        clearInterval(timeInterval); ";
        webContent += "    }";
        
        webContent += "  </script>";
        webContent += "</head>";
        webContent += "<body>";
        webContent += "  <span>Pressure : </span><span id='pressureValue'></span><br/>";
        webContent += "  <button id='trackOnBtn' onclick='startTracking();'>On</button>";
        webContent += "  <button id='trackOffBtn' onclick='stopTracking();'>Off</button>";
        webContent += "</body>";
        webContent += "</html>";
        
        server.send(200, "text/html", webContent);
    });

    server.begin();

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
}

void loop() {
    webSocket.loop();
    server.handleClient();
}
