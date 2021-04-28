//Display Libraries
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// Wifi libraries
#include <WiFiConnect.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//WiFiManager libraries
#include <WC_AP_HTML.h>
#include <WiFiConnectParam.h>
#include <WiFiClient.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266HTTPClient.h>
#else
#include <HTTPClient.h>
#endif

//SocketIO library
#include <SocketIoClient.h>

//To parse incoming json objects and format outgoing ones
#include <Arduino_JSON.h>

//Store additional data
#include <EEPROM.h>

#define USE_SERIAL Serial
//#define DEVICE_STATES_LENGTH 128
//#define BUS_OPTIONS_LENGTH 8
#define DATA 15
#define RESET 12
#define NO_LEDS 32

//EEPROM addresses
#define SSID_ADDR 32        //ssid address in EEPROM
#define PASSWORD_ADDR 64    //WiFi password address in EEPROM
#define HOST_IP_ADDR 256    //TA IP address in EEPROM
#define IP_MAXLEN 8
#define HOST_PORT_ADDR 264  //TA port in EEPROM (should be 4455)
#define PORT_MAXLEN 4       //size of int
#define CLIENT_IP_ADDR 272  //Address where client ip is stored
#define CLIENT_GW_ADDR 280  //address where client gw is stored
#define CLIENT_SN_ADDR 288  //address where client sn is stored.

#define IP_MAXLEN_STR 16    //max allowable string length of ip addresses in config portal


// Create the WifiMulti and webSocket objects
WiFiConnect wc;

SocketIoClient webSocket;

// For internet connection
WiFiClient client;
HTTPClient http;

// global variables of the device states and bus options
JSONVar device_states;
int len_device_states = 0;
JSONVar bus_options;
int len_bus_options = 0;

// initialise the string objects for preview and program
String PVW = String("\"preview\"");
String PGM = String("\"program\"");

// initialise deviceid
char *deviceId = "unassigned";

// Initialise preview and program to be false
bool mode_preview = false;
bool mode_program = false;



/*
   Initialises the dimensions of the FeatherWing LED matrix and creates a matrix object
   NeoMatrix(int width, int height, int pinNo, led0UD + led0LR + ROW/COLMajor + PROGRESSIVE/zigzag);
*/
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 4, DATA,
                            NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                            NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);



//custom params
WiFiConnectParam hostip("hostip", "hostip", "", 16);
WiFiConnectParam hostport("hostport", "hostport", "", 6);
WiFiConnectParam client_ip("clientip", "clientip", "", 16);
WiFiConnectParam gw_ip("gateway", "gateway", "", 16);
WiFiConnectParam sn_mask("subnet", "subnet", "", 16);
/*

   ############################
   WIFICONNECT FUNCTION CALLS
   ############################

*/

void configModeCallback(WiFiConnect *mWiFiConnect) {
  Serial.println("WIFI Reset. Entering Access Point");
}

void startWiFi(boolean showParams = false) {

  wc.setDebug(true);

  /* Set our callbacks */
  wc.setAPCallback(configModeCallback);

  //wc.resetSettings(); //helper to remove the stored wifi connection, comment out after first upload and re upload

  /*
    AP_NONE = Continue executing code
    AP_LOOP = Trap in a continuous loop - Device is useless
    AP_RESET = Restart the chip
    AP_WAIT  = Trap in a continuous loop with captive portal until we have a working WiFi connection
  */

  String ssid;
  String pw;
  EEPROM.begin(512);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASSWORD_ADDR, pw);
  if (!wc.autoConnect(ssid.c_str(), pw.c_str(), WIFI_STA)) { // try to connect to wifi
    
    //add parameters
    wc.addParameter(&hostip);
    wc.addParameter(&hostport);
    wc.addParameter(&client_ip);
    wc.addParameter(&gw_ip);
    wc.addParameter(&sn_mask);

    wc.startConfigurationPortal(AP_WAIT);//if not connected show the configuration portal
    //convert all the ip strings to int array
    const char* hostip_str = hostip.getValue();
    const char* clientip_str = client_ip.getValue();
    const char* gw_ip_str = gw_ip.getValue();
    const char* sn_mask_str = sn_mask.getValue();

    Serial.printf("hostip: %s", hostip_str);

    //create variables to store the info
    int len_hostip = strlen(hostip_str);
    int hostip[4];
    int clientip[4];
    int gateway[4];
    int subnet[4];

    //store the ip information in EEPROM
    ip_to_int(hostip_str, hostip);
    ip_to_int(clientip_str, clientip);
    ip_to_int(gw_ip_str, gateway);
    ip_to_int(sn_mask_str, subnet);
    Serial.printf("host ip = %d.%d.%d.%d\n", hostip[0], hostip[1], hostip[2], hostip[3]);
    IPAddress host_ip = IPAddress(hostip[0], hostip[1], hostip[2], hostip[3]);
    IPAddress client_ip_addr = IPAddress(clientip[0], clientip[1], clientip[2], clientip[3]);
    IPAddress gateway_addr = IPAddress(gateway[0], gateway[1], gateway[2], gateway[3]);
    IPAddress sn_addr = IPAddress(subnet[0], subnet[1], subnet[2], subnet[3]);
    ssid = WiFi.SSID();
    pw = WiFi.psk();
    EEPROM.begin(512);
    EEPROM.put(SSID_ADDR, ssid);
    EEPROM.put(PASSWORD_ADDR, pw);
    EEPROM.put(HOST_IP_ADDR, host_ip);
    EEPROM.put(CLIENT_IP_ADDR, client_ip_addr);
    EEPROM.put(CLIENT_GW_ADDR, gateway_addr);
    EEPROM.put(CLIENT_SN_ADDR, sn_addr);
    //write the host port to eeprom as an int
    const char* hostport_str = hostport.getValue();
    Serial.printf("hostport: %s", hostport_str);
    int len_hostport = strlen(hostport_str);
    int hostport = atoi(hostport_str);
    EEPROM.put(HOST_PORT_ADDR, hostport);
    EEPROM.commit();
  }
  //Serial.println(wc.host_ip);
  //testConnectionToTallyServer();
}

void ip_to_int(const char* ip, int arr[]) {
  uint8_t ipidx = 0;
  for (int i = 0; i < 4; i++) {
    char* field = (char *)calloc(4, sizeof(char));
    uint8_t fieldidx = 0;
    while (ip[ipidx] != '.') {
      if (ip[ipidx] == '\0') {
        break;

      }
      field[fieldidx++] = ip[ipidx++];
    }
    ipidx++;
    arr[i] = atoi(field);
    Serial.printf("field %d\n", arr[i]);
    free(field);
  }
}

/*

   ###########################
   SOCKETIO FUNCTION CALLS
   ###########################

*/

/* On connection to TallyArbiter, the device will connect to the webserver and does the following:
      - bus_options
        - gets back an array of json objects that are the bus options as preview and program
      - send its device and device states
        - gets back an array of devices as configured in the tallyArbiter web server

*/
void on_connect(const char * message, size_t length) {
  USE_SERIAL.println("on_connect: ");

  USE_SERIAL.println("Connected to server!");

  displayConnected();

  //get bus options from server
  webSocket.emit("bus_options");

  //send the type of listener this is
  //maybe can set the deviceid using dipswitches
  //JSONVar device = JSON.parse("{\"deviceId\": \"unassigned\"}");
  webSocket.emit("device_listen_esp8266", "{\"deviceId\": \"unassigned\"}");
}


void on_device_states(const char * message, size_t length) {
  /*
      data = [
     #CAM1
     {"deviceId":"0e257ab9","busId":"e393251c","sources":["851f642c"],"active":true},
     {"deviceId":"0e257ab9","busId":"334e4eda","sources":["851f642c"],"active":true},


     {"deviceId":"87bd4b40","busId":<PVW>,"sources":[???]},
     {"deviceId":"87bd4b40","busId":<PGM>,"sources":[???]}
     ]
  */


  JSONVar data = JSON.parse(message);
  device_states = data;
  USE_SERIAL.println(device_states);

  processTallyData();
}


void on_bus_options(const char * message, size_t length) {
  /*
     data = [
      {"id":"e393251c","label":"Preview","type":"preview"},
      {"id":"334e4eda","label":"Program","type":"program"}
      ]
  */

  JSONVar data = JSON.parse(message);
  bus_options = data;
  //printJson(data);
}


void on_flash(const char * payload, size_t length) {

  strobeSolidColour(500, 3, 0, 0, 255);
  evaluateMode();

}


void on_reassign(const char * payload, size_t length) {

  int commaIndex = -1;
  int len_payload = strlen(payload);

  //find the comma in the string and split the string
  for (int i = 0; i < len_payload; i++) {

    if (',' == payload[i]) {
      commaIndex = i;
      break;
    }
  }

  char *oldId = (char*)malloc(sizeof(char) * (commaIndex - 1));
  char *newId = (char*)malloc(sizeof(char) * (len_payload - commaIndex - 1));
  USE_SERIAL.println(commaIndex);

  //copy elements one by one to new string oldId
  for (int i = 0; i < commaIndex - 1; i++) {
    oldId[i] = payload[i];
  }
  oldId[commaIndex - 1] = '\0';
  USE_SERIAL.println(oldId);

  //copy elements one by one to new string newId
  int new_start = commaIndex + 2;
  for (int i = new_start; i < strlen(payload); i++) {
    newId[i - (new_start)] = payload[i];
  }
  newId[len_payload - new_start] = '\0';
  USE_SERIAL.println(newId);

  deviceId = newId;
  JSONVar data_to_send;
  data_to_send[0] = oldId;
  data_to_send[1] = newId;
  USE_SERIAL.println(JSON.stringify(data_to_send));
  String data_str = JSON.stringify(data_to_send);
  const char* data = data_str.c_str();
  webSocket.emit("listener_reassign_esp8266", data);
  // REMEMBER TO add lines in tallyarbiter server dealing with listener reassign for NodeMCUs.

  free(oldId);
}


String getBusTypeById(const char *busID) {
  /*
     for bus in bus_options:
        if bus['id'] == busId:
            return bus['type']
  */
  USE_SERIAL.println("getBusTypeByBusId: ");
  USE_SERIAL.println(busID);
  JSONVar busID_json = JSON.parse(busID);
  USE_SERIAL.println(busID_json);
  uint8_t len_bus_options = bus_options.length();
  USE_SERIAL.print("Bus Options: ");
  printJson(bus_options);


  for (uint8_t i = 0; i < len_bus_options; i++) {

    JSONVar key = bus_options[i].keys();
    USE_SERIAL.println("bus_options.keys: ");
    printJson(key);
    USE_SERIAL.printf("searching bus_options for matching key: %d \n", i);

    for (uint8_t j = 0; j < key.length(); j++) {
      USE_SERIAL.printf("bus_options[%u][%u]: ", i, j);
      USE_SERIAL.println(bus_options[i][key[j]]);

      if (bus_options[i][key[j]] == busID_json) {
        String typ = JSON.stringify(bus_options[i]["type"]);
        return typ;
      }
    }
  }
}


void printJson(JSONVar data) {

  USE_SERIAL.print("JSONVar is a ");
  USE_SERIAL.println(JSON.typeof(data));
  USE_SERIAL.print("Message: ");
  USE_SERIAL.println(data);

}


void processTallyData() {
  USE_SERIAL.println("processTallyData: ");
  for (uint8_t i = 0; i < device_states.length(); i++) {
    printJson(device_states);
    if (device_states[i].hasOwnProperty("busId")) {
      JSONVar id_jsonvar = device_states[i]["busId"];
      String id_str = JSON.stringify(id_jsonvar);
      const char *id = id_str.c_str();
      String bus_type_return = getBusTypeById(id);
      USE_SERIAL.println(bus_type_return);
      USE_SERIAL.println(PVW);
      USE_SERIAL.println(PGM);

      if (bus_type_return == PVW) {
        USE_SERIAL.println("bus type is preview.");
        if (device_states[i]["sources"].length() > 0) {
          mode_preview = true;
        }
        else {
          mode_preview = false;
        }
      }
      else if (bus_type_return == PGM) {
        USE_SERIAL.println("bus type is program.");
        printJson(device_states[i]["sources"]);
        if (device_states[i]["sources"].length() > 0) {
          mode_program = true;
        }
        else {
          mode_program = false;
        }
      }
    }
  }
  USE_SERIAL.print("Preview: ");
  USE_SERIAL.println(mode_preview);

  USE_SERIAL.print("Program: ");
  USE_SERIAL.println(mode_program);
  evaluateMode();
}


void evaluateMode() {

  if (mode_preview && !mode_program) {
    // Preview
    displaySolidColour(0, 255, 0);
  }
  else if (!mode_preview && mode_program) {
    // Program
    displaySolidColour(255, 0, 0);
  }
  else if (mode_preview && mode_program) {
    // Preview and Program
    displaySolidColour(255, 255, 0);
  }
  else {
    // Not on preview nor program. Clear screen.
    displayClear();
  }
}


/*

   #########################
   DISPLAY HELPER FUNCTIONS
   #########################

*/
// Clears the display
void displayClear() {
  displaySolidColour(0, 0, 0);
}


// Displays the matrix as a solid colour
void displaySolidColour(uint8_t red, uint8_t green, uint8_t blue) {
  for (uint8_t i = 0; i < NO_LEDS; i++) {
    matrix.setPixelColor(i, red, green, blue);
  }
  matrix.show();
}


// Strobes the solid colour
void strobeSolidColour(int delayTime, uint8_t reps, uint8_t red, uint8_t green, uint8_t blue) {
  for (uint8_t i = 0; i < reps; i++) {
    displaySolidColour(red, green, blue);
    delay(delayTime);
    displayClear();
    delay(delayTime);

  }
}


// Display Connected Pattern
void displayConnected() {
  displaySolidColour(0, 0, 0);
  for (uint8_t i = 0; i < NO_LEDS; i++) {
    uint8_t mod3 = i % 3;
    switch (mod3) {
      case 0:
        //Red
        matrix.setPixelColor(i, 255, 0, 0);
        break;

      case 1:
        //Green
        matrix.setPixelColor(i, 0, 255, 0);
        break;

      case 2:
        //Blue
        matrix.setPixelColor(i, 0, 0, 255);
        break;
    }
    matrix.show();
    delay(10);
  }
  strobeSolidColour(100, 3, 0, 255, 0);
  displayClear();
}



// SETUP AND LOOP
void setup() {
  USE_SERIAL.begin(115200);

  USE_SERIAL.setDebugOutput(false);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  pinMode(RESET, INPUT);

  //Network settings from EEPROM
  IPAddress default_client_ip = IPAddress(192, 168, 1, 191);

  IPAddress host = IPAddress(192, 168, 1, 100);
  IPAddress ip = default_client_ip;
  IPAddress gw = IPAddress(192, 168, 1, 1);
  IPAddress sn = IPAddress(255, 255, 255, 0);


  //write ip gw sn and host to variables from eeprom
  EEPROM.begin(512);
  EEPROM.get(CLIENT_IP_ADDR, ip);
  EEPROM.get(CLIENT_GW_ADDR, gw);
  EEPROM.get(CLIENT_SN_ADDR, sn);
  EEPROM.get(HOST_IP_ADDR, host);



  wc.setSTAStaticIPConfig(ip, gw, sn);
  WiFi.persistent(true);
  
  startWiFi();
  EEPROM.begin(512);
  EEPROM.get(CLIENT_IP_ADDR, ip);
  EEPROM.get(CLIENT_GW_ADDR, gw);
  EEPROM.get(CLIENT_SN_ADDR, sn);
  EEPROM.get(HOST_IP_ADDR, host);

  Serial.println(ip);
  Serial.println(gw);
  Serial.println(sn);
  Serial.println("^^ ip gw sn from EERPOM");
  wc.setSTAStaticIPConfig(ip, gw, sn);
  wc.autoConnect();

  //    pinMode(DATA, OUTPUT);

  // Begin the serial out for the matrix and set to all OFF
  matrix.begin();
  displaySolidColour(0, 0 , 0);


  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  webSocket.on("connect", on_connect);
  webSocket.on("bus_options", on_bus_options);
  webSocket.on("device_states", on_device_states);
  webSocket.on("reassign", on_reassign);
  webSocket.on("flash", on_flash);
  webSocket.begin("192.168.1.23", 4455);

}

void loop() {
  webSocket.loop();

  //Start Portal Again if wifi dies
  if (WiFi.status() != WL_CONNECTED) {
    
    if (!wc.autoConnect()) startWiFi();
      delay(5000);
  }
  

  if (digitalRead(RESET) == HIGH) {
    //Reset button pressed. reset the ssid and pw, reboot.
    String ssid = "";
    String pw = "";
    EEPROM.begin(512);
    EEPROM.put(SSID_ADDR, ssid);
    EEPROM.put(PASSWORD_ADDR, pw);
    EEPROM.commit();
    ESP.restart();
  }
}
