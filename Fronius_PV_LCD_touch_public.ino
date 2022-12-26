// Autor:   Ingmar Stapel
// Datum:   20221226
// Version: 0.3 alpha
// Homepage: https://www.blogyourearth.com
// This programm shows the main KPIs of a Fronius solar system.
// Two buttons are implemented to control a WIFI AC witch on basis of a tasmota system.
// This is a very early state of development and only shows how such a monitor could be implemented
// Many thanks go to the following blog for all the inspiration and know how how to access the Fronius API:
// https://www.onderka.com/hausautomation-technik-und-elektronik/nodemcu-json-daten-von-fronius-wechselrichter-einlesen/
#include <ArduinoJson.h>        // Arduino JSON (5.x stable, nicht 6.x)
#include <ESP8266WiFi.h>        // ESP8266 WiFi
#include <WiFiClient.h>         // ESP8266 Wifi-Client
#include <ESP8266HTTPClient.h>  // ESP8266 HTTP-Client
#include <Wire.h>               // Wire / I2C
#include <DebounceEvent.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
//#include <Adafruit_ILI9341.h>
#include <Adafruit_ILI9341esp.h>
#include <XPT2046.h>

//#define TFT_CS 4 //  D2 = GPIO 4
//#define TFT_RST 0 // D3 = GPIO 0
//#define TFT_DC 2 //  D4 = GPIO 2
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define TFT_DC 2
#define TFT_CS 15

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046 touch(/*cs=*/ 4, /*irq=*/ 5);

Adafruit_GFX_Button button_pump;
Adafruit_GFX_Button button_stove;




HTTPClient sender;
WiFiClient wifiClient;


// WiFi SSID und PSK

const char *ssid = "meine_ssid";
const char *password = "mein_psk";
const char *host_name = "esp_pva_01";

// Wechselrichter Hostname oder IP-Adresse
//const String inverterHostname = "symo.local,lan";
// Dach - Smart-Meter
const String inverterHostnameDach = "192.168.2.32";
// Erker
const String inverterHostnameErker = "192.168.2.33";

// IP Snonoff Pumpe
const String HostnamePumpe = "192.168.178.186";
String urlPumpeOff = "http://" + HostnamePumpe + "/cm?cmnd=Power%20off";
String urlPumpeOn = "http://" + HostnamePumpe + "/cm?cmnd=Power%20on";

// IP Snonoff Ofen
const String HostnameOfen = "";

// Fronius Solar API URLs "Meter Data" und "Flow Data"
String urlMeter = "http://" + inverterHostnameDach + "/solar_api/v1/GetMeterRealtimeData.cgi?Scope=Device&DeviceId=0";
String urlFlow  = "http://" + inverterHostnameDach + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
// Fronius Erker API URLs
String urlFlowErker  = "http://" + inverterHostnameErker + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
// Fronius Solar API URLs "Storage Realtime Data"
String urlStorage  = "http://" + inverterHostnameDach + "/solar_api/v1/GetStorageRealtimeData.cgi?Scope=System";


int counter = 0;
int update_counter = -1;
int pumpe_stat = 1;

// set the speed down button
#define BUTTON_PUMP          3 // D2

DebounceEvent button_de_p = DebounceEvent(BUTTON_PUMP, BUTTON_SWITCH | BUTTON_SET_PULLUP);
int button_de_p_counter = 0;

// set the speed up button
#define BUTTON_STOVE         5 // D1
DebounceEvent button_de_s = DebounceEvent(BUTTON_STOVE, BUTTON_SWITCH | BUTTON_SET_PULLUP);
int button_de_s_counter = 0;

// setup()
void setup() {
    // 1s warten
    delay(1000);

    //Serial.begin(9600);

    Serial.begin(115200);
    SPI.setFrequency(ESP_SPI_FREQ);

      
    tft.begin();
    touch.begin(tft.width(), tft.height());  // Must be done before setting rotation
    Serial.print("tftx ="); Serial.print(tft.width()); Serial.print(" tfty ="); Serial.println(tft.height());
    
    tft.fillScreen(ILI9341_BLACK);
    tft.setRotation(1);
    tft.setCursor(2, 2); 
    tft.setTextColor(ILI9341_GREEN);  tft.setTextSize(2);

    // WiFi aus
    WiFi.mode(WIFI_OFF);
    // 1s warten
    delay(1000);
    // WiFi Client-Modus, AP-Modus aus
    WiFi.mode(WIFI_STA);
    // Hostname setzen, falls gewünscht. Default ist "ESP_[MACADR]"
    //WiFi.hostname(host_name);
    // WiFi verbinden
    WiFi.begin(ssid, password);
    tft.print("Verbinde WiFi: ");

    Serial.println();
    Serial.println("ESP8266 WiFi-Client für Fronius Solar Api V1");
    Serial.println();
    Serial.print("Verbinde ");

    // Auf Verbindung warten
    while (WiFi.status() != WL_CONNECTED) {
      // Alle 0.5s einen Punkt
      delay(500);
      Serial.print(".");
      tft.print(".");
    }
    Serial.println("\nVerbunden.");


    //tft.println("ESP8266 WiFi-Client für Fronius Solar Api V1");
    tft.println("\n\nWiFi verbunden\n");
    tft.print("SSID: ");
    tft.print(ssid);
    tft.print("\nIP:   ");
    tft.print(WiFi.localIP());
    tft.print("\nMAC:  ");
    tft.print(WiFi.macAddress());

    // Verbindungs-Informationen
    Serial.println();
    Serial.print("SSID:              ");
    Serial.println(ssid);
    Serial.print("Kanal:             ");
    Serial.println(WiFi.channel());
    Serial.print("Signal (RX):       ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("IP-Adresse:        ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC-Adresse:       ");
    Serial.println(WiFi.macAddress());
    Serial.println();
    // Ende setup()
    delay(2000);
    tft.fillScreen(ILI9341_BLACK);



   // touch.setCalibration(1839, 1747, 288, 252);
   // pumpe okay
   // touch.setCalibration(288, 252, 1839, 1747);
   
    // 0
    //touch.setCalibration(1826, 261, 298, 1787);    
    // 1
    //touch.setCalibration(1861, 1763, 288, 280);   
    // 2
    //touch.setCalibration(297, 1794, 1857, 280);
    // 3
    touch.setCalibration(297, 273, 1867, 1760);    
}



static uint16_t prev_x = 0xffff, prev_y = 0xffff;


// loop()
void loop() {
    // HTTP-Clients initialisieren
    HTTPClient http;
    const size_t capacityMeter = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
    const size_t capacityFlow  = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
    const size_t capacityFlowErker  = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
    const size_t capacityStorage  = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;

    // JSON-Puffer initialisieren
    DynamicJsonBuffer jsonBufferMeter(capacityMeter);
    DynamicJsonBuffer jsonBufferFlow(capacityFlow);
    DynamicJsonBuffer jsonBufferFlowErker(capacityFlowErker);
    DynamicJsonBuffer jsonBufferStorage(capacityStorage);

    if ( update_counter > 500 ){

        update_counter = 0;
        // URL #1 - Meter Data
        http.begin(urlMeter);
        int httpCodeMeter = http.GET();
        // JSON-Antwort
        String httpResponseMeter = http.getString();
        // HTTP-Client beenden
        http.end();
    
        Serial.print("URL:               ");
        Serial.println(urlMeter);
        Serial.print("HTTP Status:       ");
        Serial.println(httpCodeMeter);
        JsonObject& jsonMeter = jsonBufferMeter.parseObject(httpResponseMeter);
        if (!jsonMeter.success()) {
          // JSON nicht erfolgreich geparst
          Serial.println("JSON-Parser URL 1:       Fehler");
        } else {
          Serial.println("JSON-Parser URL 1:       OK");
        }
    
    
    
    
        // URL #2 - Flow Data Dach
        http.begin(urlFlow);
        int httpCodeFlow = http.GET();
        // JSON-Antwort
        String httpResponseFlow = http.getString();
        // HTTP-Client beenden
        http.end();
    
        Serial.print("##### URL DACH:           ");
        Serial.println(urlFlow);
        Serial.print("HTTP Status:       ");
        Serial.println(httpCodeFlow);
          JsonObject& jsonFlow = jsonBufferFlow.parseObject(httpResponseFlow);
        if (!jsonFlow.success()) {
          // JSON nicht erfolgreich geparst
          Serial.println("JSON-Parser URL 2:       Fehler");
        } else {
          Serial.println("JSON-Parser URL 2:       OK");
        }
    
    
    
    
        // URL #3 - Flow Data Erker
        http.begin(urlFlowErker);
        int httpCodeFlowErker = http.GET();
        // JSON-Antwort
        String httpResponseFlowErker = http.getString();
        // HTTP-Client beenden
        http.end();
    
        Serial.print("##### URL ERKER:         ");
        Serial.println(urlFlowErker);
        Serial.print("HTTP Status:       ");
        Serial.println(httpCodeFlowErker);
          JsonObject& jsonFlowErker = jsonBufferFlowErker.parseObject(httpResponseFlowErker);
        if (!jsonFlowErker.success()) {
          // JSON nicht erfolgreich geparst
          Serial.println("JSON-Parser URL Erker:       Fehler");
        } else {
          Serial.println("JSON-Parser URL Erker:       OK");
        }
    
    
    
    
        // URL #4 - Storage Realtime Data
        http.begin(urlStorage);
        int httpStorage = http.GET();
        // JSON-Antwort
        String httpResponseStorage = http.getString();
        // HTTP-Client beenden
        http.end();
    
        Serial.print("URL:               ");
        Serial.println(urlStorage);
        Serial.print("HTTP Status:       ");
        Serial.println(httpStorage);
          JsonObject& jsonStorage = jsonBufferStorage.parseObject(httpResponseStorage);
        if (!jsonStorage.success()) {
          // JSON nicht erfolgreich geparst
          Serial.println("JSON-Parser URL 3:       Fehler");
        } else {
          Serial.println("JSON-Parser URL 3:       OK");
        }


    
    Serial.println();

    // Timestamp Daten Power Meter
    String ts_meter = ( jsonMeter["Head"]["Timestamp"] | "Leer" );
    // Momentanverbrauch Phase1/2/3
    int p_phase1  = ( jsonMeter["Body"]["Data"]["PowerReal_P_Phase_1"] | 0 );
    int p_phase2  = ( jsonMeter["Body"]["Data"]["PowerReal_P_Phase_2"] | 0 );
    int p_phase3  = ( jsonMeter["Body"]["Data"]["PowerReal_P_Phase_3"] | 0 );
    // Spannung Phase1/2/3
    int u_phase1  = ( jsonMeter["Body"]["Data"]["Voltage_AC_Phase_1"] | 0 );
    int u_phase2  = ( jsonMeter["Body"]["Data"]["Voltage_AC_Phase_2"] | 0 );
    int u_phase3  = ( jsonMeter["Body"]["Data"]["Voltage_AC_Phase_3"] | 0 );
    // Strom Phase1/2/3
    int i_phase1  = jsonMeter["Body"]["Data"]["Current_AC_Phase_1"].as<float>();
    int i_phase2  = jsonMeter["Body"]["Data"]["Current_AC_Phase_2"].as<float>();
    int i_phase3  = jsonMeter["Body"]["Data"]["Current_AC_Phase_3"].as<float>();
    // Momentane Leistung Summe
    int p_summe   = ( jsonMeter["Body"]["Data"]["PowerReal_P_Sum"] | 0 );
    // Momentane Blindleistung Summe
    int p_sum_ap  = ( jsonMeter["Body"]["Data"]["PowerApparent_S_Sum"] | 0 );
    // Durchschnitt Netzfrequenz
    float net_freq  = jsonMeter["Body"]["Data"]["Frequency_Phase_Average"].as<float>();

    // Timestamp Daten Power Flow
    String ts_flow  = ( jsonFlow["Head"]["Timestamp"] | "Leer" );
    // Energie Tag
    int p_day     = ( jsonFlow["Body"]["Data"]["Inverters"]["1"]["E_Day"] | 0 );
    // Energie Jahr
    int p_year    = ( jsonFlow["Body"]["Data"]["Inverters"]["1"]["E_Year"] | 0 );
    // Energie Gesamt
    int p_total   = ( jsonFlow["Body"]["Data"]["Inverters"]["1"]["E_Total"] | 0 );
    // Einspeisung / Bezug: Negativ Einspeisung, positiv Bezug
    int in_out    = ( jsonFlow["Body"]["Data"]["Site"]["P_Grid"] | 0 );
    // Verbrauch momentan
    int cons      = ( jsonFlow["Body"]["Data"]["Site"]["P_Load"] | 0 );
    // Produktion momentan
    int prod      = ( jsonFlow["Body"]["Data"]["Site"]["P_PV"] | 0 );
    // Produktion momentan Erker
    int prodErker = ( jsonFlowErker["Body"]["Data"]["Site"]["P_PV"] | 0 );
    // Autonomie (% Produktion an Verbrauch)
    int autonomy  = ( jsonFlow["Body"]["Data"]["Site"]["rel_Autonomy"] | 0 );
    // Selbstverbrauch (% Eigenverbrauch an Produktion)
    int selfcons  = ( jsonFlow["Body"]["Data"]["Site"]["rel_SelfConsumption"] | 0 );

    // StateOfCharge_Relative
    int StateOfCharge  = ( jsonStorage["Body"]["Data"]["1"]["Controller"]["StateOfCharge_Relative"] | 0 );
    // Akku entladen / laden
    int p_akku   = ( jsonFlow["Body"]["Data"]["Site"]["P_Akku"] | 0 );
    
    // Ausgabe seriell
    Serial.print("Timestamp Meter:   ");
    Serial.println(ts_meter);
    Serial.print("Timestamp Flow:    ");
    Serial.println(ts_flow);

    Serial.print("Spannung Phase 1:  ");
    Serial.print(u_phase1);
    Serial.println(" V");
    Serial.print("Spannung Phase 2:  ");
    Serial.print(u_phase2);
    Serial.println(" V");
    Serial.print("Spannung Phase 3:  ");
    Serial.print(u_phase3);
    Serial.println(" V");

    Serial.print("Strom Phase 1:     ");
    Serial.print(i_phase1);
    Serial.println(" A");
    Serial.print("Strom Phase 2:     ");
    Serial.print(i_phase2);
    Serial.println(" A");
    Serial.print("Strom Phase 3:     ");
    Serial.print(i_phase3);
    Serial.println(" A");

    Serial.print("Leistung Phase 1:  ");
    Serial.print(p_phase1 * -1);
    Serial.println(" W");
    Serial.print("Leistung Phase 2:  ");
    Serial.print(p_phase2 * -1);
    Serial.println(" W");
    Serial.print("Leistung Phase 3:  ");
    Serial.print(p_phase3 * -1);
    Serial.println(" W");

    Serial.print("Leistung Summe:    ");
    Serial.print(p_summe * -1);
    Serial.println(" W");
    Serial.print("Scheinleistung:    ");
    Serial.print(p_sum_ap);
    Serial.println(" W");

    Serial.print("Netzfrequenz:      ");
    Serial.print(net_freq);
    Serial.println(" Hz");

    Serial.print("Produktion Tag:    ");
    Serial.print(p_day / 1000);
    Serial.println(" kWh");
    Serial.print("Produktion Jahr:   ");
    Serial.print(p_year / 1000);
    Serial.println(" kWh");
    Serial.print("Produktion Gesamt: ");
    Serial.print(p_total / 1000);
    Serial.println(" kWh");


    Serial.print("StateOfCharge Relative:  ");
    Serial.print(StateOfCharge);

    
    Serial.println();

    // Negativ Einspeisung, positiv Bezug
    if ( in_out >= 0) {
      // Bezug
      Serial.print("Strom Bezug:       ");
      Serial.print(in_out);
      Serial.println(" W");
      Serial.println("Strom Einspeisung: 0.00 W");
    } else {
      // Einspeisung
      Serial.println("Strom Bezug:       0.00 W");
      Serial.print("Strom Einspeisung: ");
      Serial.print(in_out * -1);
      Serial.println(" W");
    }


    Serial.print("Verbrauch Haus:    ");
    Serial.print(cons * -1);
    Serial.println(" W");
    Serial.print("Leistung PV Dach:       ");
    Serial.print(prod);
    Serial.println(" W");
    Serial.print("Leistung PV Erker:       ");
    Serial.print(prodErker);
    Serial.println(" W");
    Serial.print("Autonomie:         ");
    Serial.print(autonomy);
    Serial.println(" %");
    Serial.print("Eigenverbrauch:    ");
    Serial.print(selfcons);
    Serial.println(" %");
    Serial.println();

    // to delet characters no longer need fill the display every 5 min
    // totally black
    if (counter > 300) {
      counter = 0;
      tft.fillScreen(ILI9341_BLACK);    

      // If there is a connection problem detected by a 0 value for 
      // the battery status restart the ESP system. Check only every 5 min.
      if (StateOfCharge < 3) {
        Serial.print("############## RESTART ##############");
        ESP.restart();
      }    
    }
    // This counter will be incremented every second by 1.
    // It is used to fill the screen black every 5 min and
    // to check if a restart of the ESP is needed.
    counter = counter + 1;

    // Now draw the values on the LCD display
    tft.setCursor(0, 2); 
    tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(2); 
    
    tft.print("Verbrauch Haus:   ");
    tft.print(abs(cons));
    tft.print(".0 ");
    tft.setCursor(290, 2);
    tft.print(" W\n");
    tft.print("Produktion Dach:  ");
    tft.print(prod);
    tft.print(".0 ");
    tft.setCursor(290, 18);
    tft.print(" W\n");

    tft.print("Produktion Erker: ");
    tft.print(prodErker);
    tft.print(".0 ");
    tft.setCursor(290, 34);
    tft.print(" W\n");
    
    // Negativ Einspeisung, positiv Bezug
    if ( in_out >= 0) {
      // Bezug
      tft.print("Bezug Netz:       ");
      tft.print(in_out);
      tft.print(".0 ");      
      tft.setCursor(290, 50);
      tft.print(" W\n");
      tft.print("Einspeisg. Netz:  0.0");
      tft.setCursor(290, 64);
      tft.print(" W\n");      
    } else {
      // Einspeisung
      tft.print("Bezug Netz:       0.0");
      tft.setCursor(290, 50);
      tft.print(" W\n");      
      tft.print("Einspeisg. Netz:  ");
      tft.print(in_out * -1);
      tft.print(".0 ");
      tft.setCursor(290, 64);
      tft.print(" W\n");
    }

    tft.setCursor(0, 82);
    // Inform the user if the battery is charging or discharging
    if (p_akku <= 0){
      tft.print("Bat. laden:       ");
      tft.print(p_akku * -1);
      tft.print(".0 ");
      tft.setCursor(290, 82);
      tft.print(" W\n");
    }

    if (p_akku > 0){
      tft.print("Bat. entladen:   ");
      tft.print(p_akku * -1);
      tft.print(".0 ");
      tft.setCursor(290, 82);
      tft.print(" W\n");
    }
            

  // draw the line for the battery status information.
    tft.drawLine(0, 100, 320, 100, ILI9341_GREEN);
    tft.setCursor(2, 106);
    tft.setTextColor(ILI9341_BLUE, ILI9341_BLACK);  tft.setTextSize(4);
    tft.print(" BAT ");

    if (StateOfCharge < 70) {
      tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);  tft.setTextSize(4);
    } 
    if (StateOfCharge < 50){
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
    }
    if (StateOfCharge >= 70) {
      tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(4);
    }

    tft.print(StateOfCharge);
    tft.print(".0 %");      
    

    // horizontal line
    tft.drawLine(0, 140, 320, 140, ILI9341_GREEN);
    // vertical line 
    tft.drawLine(160, 140, 160, 280, ILI9341_GREEN);
    

    // Now the switch is implemented to control the pump in the house
    // via a SONOFF POW R2 switch with Tasmota.
    if (pumpe_stat == 1) 
    {
        if (sender.begin(wifiClient, urlPumpeOff)) {
          button_pump.initButton(&tft, 75, 210, 120, 50, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "EIN", 4);
          button_pump.drawButton();
          tft.setCursor(2, 150);      
          // HTTP-Code der Response speichern
          int httpCode = sender.GET();
         
          if (httpCode > 0) {
            
            // Anfrage wurde gesendet und Server hat geantwortet
            // Info: Der HTTP-Code für 'OK' ist 200
            if (httpCode == HTTP_CODE_OK) {
      
              // Hier wurden die Daten vom Server empfangen
              // String vom Webseiteninhalt speichern
              String payload = sender.getString();
      
              // Hier kann mit dem Wert weitergearbeitet werden
             // ist aber nicht unbedingt notwendig
              Serial.println(payload);                      
            }
            
          }else{
            // Falls HTTP-Error
            Serial.printf("HTTP-Error: ", sender.errorToString(httpCode).c_str());
            tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
            tft.print("ERROR:\n");
          }
      
          // Wenn alles abgeschlossen ist, wird die Verbindung wieder beendet
          sender.end();
          
        }else {
          Serial.printf("HTTP-Verbindung konnte nicht hergestellt werden!");
          tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
          tft.print("ERROR:\n");
        }
    }
    else {
        //  pumpe_stat = 1;
  
          if (sender.begin(wifiClient, urlPumpeOn)) {

            button_pump.initButton(&tft, 75, 210, 120, 50, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "AUS", 4);
            button_pump.drawButton(); 
            tft.setCursor(2, 150);
            // HTTP-Code der Response speichern
            int httpCode = sender.GET();
           
        
            if (httpCode > 0) {
              
              // Anfrage wurde gesendet und Server hat geantwortet
              // Info: Der HTTP-Code für 'OK' ist 200
              if (httpCode == HTTP_CODE_OK) {
        
                // Hier wurden die Daten vom Server empfangen
                // String vom Webseiteninhalt speichern
                String payload = sender.getString();
        
                // Hier kann mit dem Wert weitergearbeitet werden
               // ist aber nicht unbedingt notwendig
                Serial.println(payload);               
                
              }
              
            }else{
              // Falls HTTP-Error
              Serial.printf("HTTP-Error: ", sender.errorToString(httpCode).c_str());
              tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
              tft.print("ERROR:\n");
            }
        
            // Wenn alles abgeschlossen ist, wird die Verbindung wieder beendet
            sender.end();
            
          }else {
            Serial.printf("HTTP-Verbindung konnte nicht hergestellt werden!");
            tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
            tft.print("ERROR:\n");
          }
  
    }
    


    // Ofen Ansteuerung in Automatik
    if (button_de_s_counter == 0) {
    // Negativ Einspeisung, positiv Bezug
      if (( in_out >= 800) && (StateOfCharge >= 99)){
  
        tft.setCursor(170, 150);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print("Ofen");
        tft.setCursor(195, 200);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print(" AN ");
      } else {
        tft.setCursor(170, 150);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print("Ofen");
        tft.setCursor(195, 200);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print("AUS");
      }    
    }
    
    // Ofen Ansteuerung manuell eingeschalten
    if (button_de_s_counter == 1) {
        tft.setCursor(170, 150);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print("Ofen");
        tft.setCursor(195, 200);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(4);
        tft.print(" AN ");
      }   
        
    // Ofen Ansteuerung manuell ausgeschalten
    if (button_de_s_counter == 2) {
      tft.setCursor(170, 150);
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
      tft.print("Ofen");
      tft.setCursor(195, 200);
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);  tft.setTextSize(4);
      tft.print("AUS");
    }    

    button_stove.initButton(&tft, 240, 210, 120, 50, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "EIN", 4);
    button_stove.drawButton();




    // Now the operating mode of the stove is set. The user is able to control the mode
    // via a push button (auto - manual on/off)
    if (button_de_s.loop() != EVENT_NONE) {
      Serial.println("stove");
      Serial.println(button_de_s_counter);
      if (button_de_s_counter == 0) {
        button_de_s_counter = button_de_s_counter + 1;
      }
      else if (button_de_s_counter == 1) {
        button_de_s_counter = button_de_s_counter + 1;
      }   
      else if (button_de_s_counter == 2) {
        button_de_s_counter = 0;
      }
    }
    // Ende if counter
   }

  
;
    // Set the repeat time for loading / rading the API here
    // 1 second seems to be a good value.

  uint16_t x, y;
  if (touch.isTouching()) {
    touch.getPosition(x, y);
    prev_x = x;
    prev_y = y;
  } else {
    prev_x = prev_y = 0xffff;
  }
  
  
  button_pump.press(button_pump.contains(x, y)); // tell the button it is pressed
  button_stove.press(button_stove.contains(x, y)); // tell the button it is pressed  

// now we can ask the buttons if their state has changed
  if (button_pump.justReleased()) {
    button_pump.drawButton(); // draw normal
    Serial.printf("------ Button Pumpe losgelassen");
  }

  if (button_pump.justPressed()) {
    button_pump.drawButton(true); // draw invert!
    if (pumpe_stat == 1) 
    {
      pumpe_stat = 0;    
    }
    else{
      pumpe_stat = 1;     
    }
    Serial.printf("------ Button Pumpe gedrückt");
  }


  if (button_stove.justReleased()) {
    button_stove.drawButton(); // draw normal
    Serial.printf("------ Button Ofen losgelassen");
  }

  if (button_stove.justPressed()) {
    button_stove.drawButton(true); // draw invert!
    Serial.printf("------ Button Ofen gedrueckt");
  }



if ((update_counter % 2) == 0)
{
  tft.setCursor(300, 200);
          tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(2);
        tft.print(" - ");
}
else
{
  tft.setCursor(300, 200);
          tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);  tft.setTextSize(2);
        tft.print(" | ");
}

    
    delay(10);
    update_counter = update_counter + 1;
    // Ende loop()
}

// EOF
