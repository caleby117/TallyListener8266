//Display Libraries
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// Wifi libraries
#include <WiFiConnect.h>

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
void displayConnecteed();

/*
 * TODO
 * 1. Make the ESP microcontroller communicate all the lighting commands to the Featherwing
 *   a. IMPORTANT: Show correct tally information on the Featherwing DONE
 *   b. OPTIONAL: Set the different lighting codes
 * 
 * 2. Integrate WifiManager into the codebase DONE
 * 
 * 3. Edit WifiManager's header files to make it possible to the settings and deviceid for first time setup.  
 *   a. Make it possible to input static IP of the device on the network from the captive portal DONE
 *   b. Make it possible to input TallyServer IP from the captive portal
 *   c. and save into storage so that it will remember.
 *
 */


/* On startup, the device will connect to the webserver and does the following:
 *    - bus_options
 *      - gets back an array of json objects that are the bus options as preview and program
 *    - send its device and device states
 *      - gets back an array of devices as configured in the tallyArbiter web server
 * 
 */

#define USE_SERIAL Serial
//#define DEVICE_STATES_LENGTH 128
//#define BUS_OPTIONS_LENGTH 8
#define POWER 4
#define DATA 15
#define NO_LEDS 32

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


// Brightness - 10 different brightness stages
double brightness = 0.5;


/*
 * Initialises the dimensions of the FeatherWing LED matrix and creates a matrix object
 * NeoMatrix(int width, int height, int pinNo, led0UD + led0LR + ROW/COLMajor + PROGRESSIVE/zigzag);
 */
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 4, DATA, 
      NEO_MATRIX_TOP + NEO_MATRIX_LEFT + 
      NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);



/*
 * 
 * ############################
 * WIFIMANAGER FUNCTION CALLS
 * ############################
 * 
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
    if (!wc.autoConnect()) { // try to connect to wifi
        /* We could also use button etc. to trigger the portal on demand within main loop */
        wc.startConfigurationPortal(AP_WAIT);//if not connected show the configuration portal
    }
    //Serial.println(wc.host_ip);
    //testConnectionToTallyServer();
}

uint8_t testConnectionToTallyServer(){

    USE_SERIAL.println("Testing connection to the Tally Server");
    http.begin(client, "192.168.1.112:4455/");

    int httpCode = http.GET();

    if (httpCode > 0){
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      
        // file found at server
        if (httpCode == HTTP_CODE_OK) 
        {
            Serial.println("HTTP_CODE_OK");
        }
        http.end();
        return 0;
      } 
     else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return 1;        
    }
}

/*
 * 
 * ###########################
 * SOCKETIO FUNCTION CALLS
 * ###########################
 * 
 */
// Called on successful connection to the TallyArbiter Server
void on_connect(const char * message, size_t length){
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


void on_device_states(const char * message, size_t length){
    /*
     *  data = [
     * #CAM1
     * {"deviceId":"0e257ab9","busId":"e393251c","sources":["851f642c"],"active":true},
     * {"deviceId":"0e257ab9","busId":"334e4eda","sources":["851f642c"],"active":true},
     * 
     * 
     * {"deviceId":"87bd4b40","busId":<PVW>,"sources":[???]},
     * {"deviceId":"87bd4b40","busId":<PGM>,"sources":[???]}
     * ]
     */

    
    JSONVar data = JSON.parse(message);
    device_states = data;
    USE_SERIAL.println(device_states); 

//    reset_array(device_states, (int)device_states.length());
//    
//    for(int i=0; i<data.length(); i++){
//        device_states[i] = data[i];
//        USE_SERIAL.println(data[i]);
//        USE_SERIAL.println(device_states[i]);
//
//    }

    processTallyData();
}


void on_bus_options(const char * message, size_t length){
    /*
     * data = [
     *  {"id":"e393251c","label":"Preview","type":"preview"},
     *  {"id":"334e4eda","label":"Program","type":"program"}
     *  ]
     */
    
    JSONVar data = JSON.parse(message);
    bus_options = data;
//    len_bus_options = 0;
//    reset_array(bus_options, (int)device_states.length());
//    for(int i=0; i<data.length(); i++){
//        JSONVar bus_option_obj = data[i];
//        bus_options[i] = bus_option_obj;
//        len_bus_options += 1;
//
//    }
    printJson(data);
}


void on_flash(const char * payload, size_t length){

    strobeSolidColour(500, 3, 0, 0, (uint8_t)((brightness)*0xFF));
    evaluateMode();

}


void on_reassign(const char * payload, size_t length){

    int commaIndex = -1;
    int len_payload = strlen(payload);

    //find the comma in the string and split the string
    for(int i=0; i<len_payload; i++){
      
        if(',' == payload[i]){
            commaIndex = i;
            break;
        }
    }

    char *oldId = (char*)malloc(sizeof(char)*(commaIndex-1));
    char *newId = (char*)malloc(sizeof(char)*(len_payload-commaIndex-1));
    USE_SERIAL.println(commaIndex);

    //copy elements one by one to new string oldId
    for(int i=0; i<commaIndex-1; i++){
        oldId[i] = payload[i];
    }
    oldId[commaIndex-1] = '\0';
    USE_SERIAL.println(oldId);

    //copy elements one by one to new string newId
    int new_start = commaIndex+2;
    for(int i=new_start; i<strlen(payload); i++){
        newId[i-(new_start)] = payload[i];
    }
    newId[len_payload-new_start] = '\0';
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


String getBusTypeById(const char *busID){
    /*
     * for bus in bus_options:
     *    if bus['id'] == busId:
     *        return bus['type']
     */
    USE_SERIAL.println("getBusTypeByBusId: ");
    USE_SERIAL.println(busID);
    JSONVar busID_json = JSON.parse(busID);
    USE_SERIAL.println(busID_json);    
    uint8_t len_bus_options = bus_options.length();
    USE_SERIAL.print("Bus Options: ");
    printJson(bus_options);
     

    for(uint8_t i=0; i<len_bus_options; i++){
      
        JSONVar key = bus_options[i].keys();
        USE_SERIAL.println("bus_options.keys: ");
        printJson(key);
        USE_SERIAL.printf("searching bus_options for matching key: %d \n", i);
          
        for(uint8_t j=0; j<key.length(); j++){
            USE_SERIAL.printf("bus_options[%u][%u]: ", i, j);
            USE_SERIAL.println(bus_options[i][key[j]]);

            if(bus_options[i][key[j]] == busID_json){
               String typ = JSON.stringify(bus_options[i]["type"]);
               return typ;
            }
        }
    }
}


void printJson(JSONVar data){
    
    USE_SERIAL.print("JSONVar is a ");
    USE_SERIAL.println(JSON.typeof(data));
    USE_SERIAL.print("Message: ");
    USE_SERIAL.println(data);

}


void processTallyData(){
    USE_SERIAL.println("processTallyData: ");
    for(uint8_t i=0; i<device_states.length(); i++){
        printJson(device_states);
        if(device_states[i].hasOwnProperty("busId")){
            JSONVar id_jsonvar = device_states[i]["busId"];
            String id_str = JSON.stringify(id_jsonvar);
            const char *id = id_str.c_str();
            String bus_type_return = getBusTypeById(id);
            USE_SERIAL.println(bus_type_return);
            USE_SERIAL.println(PVW);
            USE_SERIAL.println(PGM);

            if(bus_type_return == PVW){
                USE_SERIAL.println("bus type is preview.");
                if(device_states[i]["sources"].length() > 0){
                    mode_preview = true;
                }
                else{
                    mode_preview = false;
                }
            }
            else if(bus_type_return == PGM){
                USE_SERIAL.println("bus type is program.");
                printJson(device_states[i]["sources"]);
                if(device_states[i]["sources"].length()>0){
                    mode_program = true;
                }
                else{
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


void evaluateMode(){
    USE_SERIAL.println((uint8_t)((brightness)*0xFF));
    if(mode_preview && !mode_program){
        // Preview
        displaySolidColour(0, (uint8_t)((brightness)*0xFF), 0);
    }
    else if(!mode_preview && mode_program){
        // Program
        displaySolidColour((uint8_t)((brightness)*0xFF), 0, 0);
    }
    else if(mode_preview && mode_program){
        // Preview and Program
        displaySolidColour((uint8_t)((brightness)*0xFF), (uint8_t)((brightness)*0xFF), 0);
    }
    else{
        // Not on preview nor program. Clear screen.
        displayClear();
    }
}


/*
 * 
 * #########################
 * DISPLAY HELPER FUNCTIONS
 * #########################
 * 
 */
// Clears the display
void displayClear(){
    displaySolidColour(0, 0, 0);
}


// Displays the matrix as a solid colour
void displaySolidColour(uint8_t red, uint8_t green, uint8_t blue){
    for(uint8_t i=0; i<NO_LEDS; i++){
        matrix.setPixelColor(i, red, green, blue);
    }
    matrix.show();
}


// Strobes the solid colour
void strobeSolidColour(int delayTime, uint8_t reps, uint8_t red, uint8_t green, uint8_t blue){
    for(uint8_t i=0; i<reps; i++){
        displaySolidColour(red, green, blue);
        delay(delayTime);
        displayClear();
        delay(delayTime);
        
    }
}


// Display Connected Pattern
void displayConnected(){
    displaySolidColour(0, 0, 0);
    for(uint8_t i=0; i<NO_LEDS; i++){
        uint8_t mod3 = i%3;
        switch(mod3){
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

char * ip_to_str(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4)
{
    char *str;
    str = (char *)malloc(sizeof(char) * 16);
    sprintf(str, "%d.%d.%d.%d\0", ip1, ip2, ip3, ip4);
    return str;
}

// SETUP AND LOOP
void setup() {
    USE_SERIAL.begin(115200);

    USE_SERIAL.setDebugOutput(false);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    pinMode(POWER, OUTPUT);

    IPAddress ip = IPAddress(192,168,0,200);
    IPAddress gw = IPAddress(192,168,1,1);
    IPAddress sn = IPAddress(255,255,0,0);

    //IPAddress ip = IPAddress(ip1, ip2, ip3, ip4);
    //IPAddress gw = IPAddress(ip1, ip2, ip3, 1);
    //IPAddress sn = IPAddress(255, 255, 255, 0); 
    
    wc.setSTAStaticIPConfig(ip, gw, sn);
    startWiFi();
    //char* host_ip = ip_to_str(wc.host_ip[0], wc.host_ip[1], wc.host_ip[2], wc.host_ip[3]);

//    pinMode(DATA, OUTPUT);

    // Begin the serial out for the matrix and set to all OFF
    matrix.begin();
    displaySolidColour(0, 0 ,0);


    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    webSocket.on("connect", on_connect);
    webSocket.on("bus_options", on_bus_options);
    webSocket.on("device_states", on_device_states);
    webSocket.on("reassign", on_reassign);
//    webSocket.on("event", event);
    webSocket.on("flash", on_flash);
    webSocket.begin("192.168.0.155", 4455);
    //digitalWrite(POWER, 50);
    //free(host_ip);
}

void loop() {
    webSocket.loop();
//    displaySolidColour(255, 0, 0);
//    Serial.println("Red");
//    delay(500);
//    displaySolidColour(0, 255, 0);
//    Serial.println("Green");
//    delay(500);
//    displaySolidColour(0, 0, 255);
//    Serial.println("Blue");
//    delay(500);

    // Wifi Dies? Start Portal Again
    if (WiFi.status() != WL_CONNECTED) {
        if (!wc.autoConnect()) wc.startConfigurationPortal(AP_WAIT);
    }

   /*
    * Implication of this working is that you can effectively add a pot here or a switch or buttons or what have you and
    * change the brightness of the LED matrix
    */

}
