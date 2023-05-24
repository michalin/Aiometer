#ifdef USE_OTA
#include <AsyncElegantOTA.h>
#endif
#include <Arduino_ConnectionHandler.h>
#include <mdns.h>
#include <Adafruit_NeoPixel.h>
#include <EasyButton.h>
#include <string>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include "vfd.h"
// #include "errors.h"

#define ENC_CLK 5
#define ENC_DT 18
#define ENC_SW 19
#define LED_PIN 32
#define EN_PIN 33
#define CLOCK_TIMER_PERIOD 1000000
#define STATUS_IDLE 0
#define STATUS_IDLE_PENDING 0x10
#define STATUS_ON 1
#define STATUS_SET 2
#define SET_LED_HUE 0
#define SET_LED_VAL 1
#define MULTIPLEX_PRIORITY 6
#define HTTP_TIMEOUT 20000
#define CURRENT_QUERY current_selected - 1
#define set_ledcolor(color)     \
    neopixel.fill(color, 0, 0); \
    neopixel.show()
#define J_COLORHSV Adafruit_NeoPixel::ColorHSV(j_settings["settings"][SET_LED_HUE]["value"], 255, j_settings["settings"][SET_LED_VAL]["value"])
#define J_COLORRGB(key) Adafruit_NeoPixel::Color(j_settings[key][1], j_settings[key][0], j_settings[key][2])
#define J_SETTING j_settings["settings"][current_selected]
#define SETTINGS_FNAME "/settings.json"

#define DEBUG                      \
    #ifdef DEBUG Serial.printf(x); \
    #endif

File json_file; // settings.json
WiFiConnectionHandler wificonnection("","");
//WiFiClientSecure *httpsClient;// = new WiFiClientSecure;
//WiFiClient *httpClient;// = new WiFiClient;
DynamicJsonDocument j_settings(10000);
String server_response;
EasyButton knob_clk(ENC_CLK, 35, false);
EasyButton knob_dt(ENC_DT, 35, false);
EasyButton knob_sw(ENC_SW, 35, false);
int current_item, items_max_index;              // Changed when knob is turned
int current_selected = 1, selections_max_index; // Changed when knob is turned while being pressed
int saved_selected = current_selected;          // Used to remember last selection before idle
int saved_item;
Adafruit_NeoPixel neopixel(8, LED_PIN);
AsyncWebServer server(80);
esp_timer_handle_t clock_timer_handle;
uint8_t status = STATUS_ON;
void clock_callback(void *);

void getCurrentItem(int query, int item)
{
    // Serial.printf("-->getCurrentItem(%d, %d)\n",query,item);
    if(server_response == "")
        return;
    JsonObject jitem = j_settings["queries"][query]["get"][item].as<JsonObject>();
    DynamicJsonDocument jresponse(ESP.getMaxAllocHeap());
    deserializeJson(jresponse, server_response);
    selections_max_index = j_settings["queries"].size();
    JsonVariant jresp = jresponse;
    while (jitem.nesting() >= 1)
    {
        for (JsonPair p : jitem)
        {
            const char *key = p.key().c_str();
            JsonVariant val = p.value();
            jitem = jitem[key];
            // Serial.printf("Pair: {\"%s\":\"%s\"}\n", p.key().c_str(), p.value().as<String>().c_str());
            // Serial.printf("Nesting: %d, key: %s\n",jresp.nesting(),key);
            if (jresp.nesting() == 1)
            {
                char buf[128];
                const char *fmt = p.value();
                sprintf(buf, fmt, jresp[key].as<String>().c_str());
                vfd_set(buf);
            }

            if (jresp.is<JsonObject>())
            {
                // Serial.printf("Object: %s\n", jresp.as<String>().c_str());
                jresp = jresp[key];
            }
            else if (jresp.is<JsonArray>())
            {
                // Serial.printf("Array: %s\n", jresp.as<String>().c_str());
                jresp = jresp[atoi(key)];
            }
            else
                Serial.printf("Error: %s\n", jresp.as<String>().c_str());

            if (jresponse.overflowed())
                Serial.println("JSON document OVERFLOW");
        }
    }
}

HTTPClient https; 
WiFiClient *wifiClient = new WiFiClient();
WiFiClient *wifiClientSecure = new WiFiClientSecure();
int get_request(String url)
{
    //Serial.printf("-->get_request(%s)\n", url.c_str());
    server_response = "";
    int httpCode;
    https.setTimeout(HTTP_TIMEOUT);
    if (url.indexOf("localhost") < 0)
    {
        set_ledcolor(J_COLORRGB("ledcolor_wait")); // Do not change led color when working as clock
    }
    String data = J_SETTING["value"].as<String>();
    if (data)
    {
        const char *url_c = url.c_str();
        char buf[100];
        sprintf(buf, url_c, data.c_str());
        url = buf;
    }
    url.replace(" ", "%20");
    // Serial.println("accessing url: " + url);*/
    if (url.startsWith("http://") ? https.begin(*wifiClient, url) : https.begin(url))
    {
        vTaskPrioritySet(xTaskGetHandle("multiplex"), 10);
        httpCode = https.GET();
        vTaskPrioritySet(xTaskGetHandle("multiplex"), MULTIPLEX_PRIORITY);
        //Serial.printf("HTTP code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            set_ledcolor(J_COLORHSV);
            server_response = https.getString();
            // Serial.println("Server response: " + server_response);
        }
        else
        {
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
            set_ledcolor(J_COLORRGB("ledcolor_error"));
            vfd_set("http err" + String(httpCode));
        }
        https.end();
    } else
        Serial.println("request failed");

    //Serial.println("get_request()-->");
    return httpCode;
}
/*
    - sel_main has changed: Dislplay alias of selection
    - sel_item changed: Display item
*/
void on_selected(int sel_main, int sel_item)
{
    // Serial.printf("-->on_selected(%d, %d)\n", sel_main, sel_item);
    if (sel_main != current_selected)
    {
        esp_timer_stop(clock_timer_handle);
        if (sel_main == 0)
        {
            vfd_set("0ff");
            status = STATUS_IDLE_PENDING;
            // return;
        }
        else
        {
            // Serial.println(j_settings["queries"][sel_main - 1]["alias"].as<String>());
            vfd_set(j_settings["queries"][sel_main - 1]["alias"].as<String>());
            status = STATUS_ON;
        }
        current_selected = sel_main;
    }
    if (sel_item != current_item) // Item selected
    {
        getCurrentItem(CURRENT_QUERY, sel_item);
        current_item = sel_item;
    }
    items_max_index = j_settings["queries"][sel_main - 1]["get"].size() - 1;
}

void on_setting(int sel_main, bool direction)
{
    // Serial.printf("-->on_setting(%d, %d)\n", sel_main, direction);
    if (sel_main != current_selected)
    {
        current_selected = sel_main;
        vfd_set(J_SETTING["alias"].as<String>());
        return;
    }
    int value = (int)J_SETTING["value"];
    value += (2 * direction - 1) * (int)J_SETTING["step"];
    value = value > (int)J_SETTING["max"] ? (int)J_SETTING["min"] : value;
    value = value < (int)J_SETTING["min"] ? (int)J_SETTING["max"] : value;
    J_SETTING["value"] = value;
    get_request(J_SETTING["url"].as<String>());
    //clock_callback(NULL);
    vfd_set(String(value));
}

/*Functions for knob user interface*/
void on_knob_released()
{
    // Serial.printf("-->on_knob_released()\n");
    switch (status)
    {
    case STATUS_IDLE_PENDING: // Go to sleep
        status = STATUS_IDLE;
        saved_item = current_item;
        digitalWrite(EN_PIN, LOW);
        set_ledcolor(Adafruit_NeoPixel::Color(0, 0, 0));
        break;
    case STATUS_IDLE: // Wake up
        status = STATUS_ON;
        current_selected = saved_selected;
        current_item = saved_item;
        clock_callback(NULL);
        selections_max_index = j_settings["queries"].size();
        esp_timer_start_periodic(clock_timer_handle, CLOCK_TIMER_PERIOD * (int)j_settings["queries"][CURRENT_QUERY]["refresh"]);
        digitalWrite(EN_PIN, HIGH);
        set_ledcolor(J_COLORHSV);
        // Serial.printf("Idle->On: Current: %d, saved: %d\n", current_item, saved_item);
        break;
    case STATUS_ON: // Selected query has changed
        saved_selected = current_selected;
        current_item = 0;
        clock_callback(NULL);
        esp_timer_start_periodic(clock_timer_handle, CLOCK_TIMER_PERIOD * (int)j_settings["queries"][CURRENT_QUERY]["refresh"]);
        // Serial.printf("On: Current: %d, saved: %d\n", current_item, saved_item);
    }
}

/* Knob turned: Set item, Knob pressed while turned: Set query*/
void on_knob_turned()
{
    // Serial.printf("-->on_knob_turned()\n");
    if (status == STATUS_IDLE)
        return;

    int sel_main = current_selected;
    int sel_item = current_item;
    if (knob_sw.isPressed()) // Pressed: select query or setting
    {
        if (knob_dt.isPressed()) // Turn right
            sel_main < selections_max_index ? sel_main++ : sel_main = 0;
        else
            sel_main > 0 ? sel_main-- : sel_main = selections_max_index;
    }
    else
    {
        if (knob_dt.isPressed()) // Turn right
            sel_item < items_max_index ? sel_item++ : sel_item = 0;
        else
            sel_item > 0 ? sel_item-- : sel_item = items_max_index;
    }
    if (status == STATUS_SET)
        on_setting(sel_main, knob_dt.isPressed());
    else
        on_selected(sel_main, sel_item);
}

/*Pressing the knob twice whithin 1s switches to settings mode and back*/
void on_knob_dblclick()
{
    // Serial.printf("-->on_knob_dblclick()\n");
    if (status == STATUS_SET)
    {
        status = STATUS_ON;
        current_item = saved_item;
        current_selected = saved_selected;

        clock_callback(NULL);
        esp_timer_start_periodic(clock_timer_handle, CLOCK_TIMER_PERIOD * (int)j_settings["queries"][CURRENT_QUERY]["refresh"]);
        selections_max_index = j_settings["queries"].size();
        json_file = SPIFFS.open(SETTINGS_FNAME, FILE_WRITE);
        serializeJsonPretty(j_settings, json_file);
        json_file.close();
        return;
    }
    esp_timer_stop(clock_timer_handle);
    saved_item = current_item;
    current_item = 0;
    current_selected = 0;
    selections_max_index = j_settings["settings"].size() - 1;
    status = STATUS_SET;
    vfd_set("ctrl");
}

/*  Called by RTOS. Reloads data at the interval specified in "refresh"*/
void clock_callback(void *arg)
{
    // Serial.println(j_settings["setttings"][CURRENT_QUERY]["url"].as<String>());
    int code = get_request(j_settings["queries"][CURRENT_QUERY]["url"].as<String>());
    if(!(code == HTTP_CODE_OK || code == HTTP_CODE_MOVED_PERMANENTLY)) //Workaround. This sometimes happens without reason.
    {
        vfd_set("wait");
        get_request(j_settings["queries"][CURRENT_QUERY]["url"].as<String>()); //Retry
    }
    getCurrentItem(CURRENT_QUERY, current_item);
}

/* responds to http access to localhost and returns current date and time
also used for configuration */
void on_http_local(AsyncWebServerRequest *request)
{
    //Serial.printf("-->on_http_local(%s):\n", request->url().c_str());
    if (request->url().equals("/time"))
    {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to obtain time");
            return;
        }
        const char *json = "{\"time\":\"%s\",\"date\":\"%s\"}";
        char time[12];
        strftime(time, 12, j_settings["timeformat"], &timeinfo);
        char date[12];
        strftime(date, 12, j_settings["dateformat"], &timeinfo);
        char buf[64];
        sprintf(buf, json, time, date);
        request->send(200, "text/plain", buf);
        return;
    }
    if (request->url().equals("/led"))
    {
        String argname = request->argName(0);
        int index;
        if (argname.equals("hue"))
            index = SET_LED_HUE;
        else if (argname.equals("value"))
            index = SET_LED_VAL;
        else
        {
            request->send(200, "text/plain", "unknown argument: " + argname);
            return;
        }
        j_settings["settings"][index]["value"] = (long)request->arg(argname).toInt();
        set_ledcolor(J_COLORHSV);
        request->send(200, "text/plain", argname + "=" + (j_settings["settings"][index]["value"].as<String>()));
        return;
    }
    if (request->url().equals("/tubes"))
    {
        request->send(200, "text/plain", "success");
    }
    request->send(200, "text/plain", "unknown");
}

void on_http_remote(AsyncWebServerRequest *request)
{
    int turn_direction = request->arg("dir").toInt();
    int pressed = request->arg("pressed").equals("true");
    // Serial.printf("-->on_http_remote(direction:%d, pressed: %d)\n", turn_direction, pressed);
    int sel_main = current_selected;
    int sel_item = current_item;
    if (pressed)
    {
        sel_main += turn_direction;
        if (sel_main > selections_max_index)
            sel_main = 0;
        if (sel_main < 0)
            sel_main = selections_max_index;
    }
    else
    {
        sel_item += turn_direction;
        if (sel_item > items_max_index)
            sel_item = 0;
        if (sel_item < 0)
            sel_item = items_max_index;
        if (turn_direction == 0)
        {
            sel_item = 0;
            on_knob_released();
        }
    }
    on_selected(sel_main, sel_item);

    DynamicJsonDocument jdoc(1024);
    jdoc["direction"] = turn_direction;
    jdoc["pressed"] = pressed;
    jdoc["vfd"] = "vfd";
    jdoc["refresh"] = j_settings["queries"][CURRENT_QUERY]["refresh"];
    request->send(200, "text/html", jdoc.as<String>());
    // Serial.printf("on_http_remote()-->\n");
}

void on_wlan_disconnected()
{
    Serial.println("-->on_wlan_disconnected()");
    set_ledcolor(J_COLORRGB("ledcolor_error"));
    vfd_set("wlan err 2");
}
void on_wlan_error()
{
    Serial.println("-->on_wlan_error()");
    set_ledcolor(J_COLORRGB("ledcolor_error"));
    vfd_set("wlan err 1");
}
void on_wlan_connected()
{
    // Serial.println("-->on_wlan_connected()");
    mdns_init();
    mdns_hostname_set(j_settings["hostname"]);
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*")); //CORS magic
    // Local webserver for time and date
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) { // Content of settings.json
        File web = SPIFFS.open("/index.htm", FILE_READ);
        request->send(200, "text/html", web.readString());
        web.close();
    });
    server.on("/time", HTTP_GET, on_http_local);
    server.on("/led", HTTP_GET, on_http_local);
    server.on("/tubes", HTTP_GET, on_http_local);
    server.on("/remote", HTTP_GET, on_http_remote);
#ifdef USE_OTA
    AsyncElegantOTA.begin(&server);
#endif
    server.begin();
    clock_callback(NULL);

    configTime(3600 * (int)j_settings["gmtoffset_h"],
               3600 * (int)j_settings["daylightoffset_h"],
               j_settings["ntpserver"]);

    const esp_timer_create_args_t clock_timer_args = {
        .callback = &clock_callback};
    esp_timer_create(&clock_timer_args, &clock_timer_handle);
    esp_timer_start_periodic(clock_timer_handle, CLOCK_TIMER_PERIOD * (int)j_settings["queries"][CURRENT_QUERY]["refresh"]);
    set_ledcolor(J_COLORHSV);
}

void setup()
{
    Serial.begin(115200);
    neopixel.begin();
    set_ledcolor(Adafruit_NeoPixel::Color(0, 0, 0));

    // Init tubes
    vfd_init();
    vfd_set("Connecting to wlan");
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, HIGH);
    xTaskCreate( // Run tube multiplexing as own realtime task with high priority
        [](void *p)
        {
            while (1)
                vfd_multiplex();
        },
        "multiplex", /* Name of the task */
        10000,       /* Stack size in words */
        NULL,        /* Task input parameter */
        MULTIPLEX_PRIORITY,
        NULL); /* Task handle. */
               // 1);   /* Core */

    // Read settings
    if (!SPIFFS.begin())
    {
        Serial.println("SPIFFS Mount Failed");
        vfd_set("spiffs err");
        set_ledcolor(J_COLORRGB("ledcolor_error"));
        return;
    }
    Serial.println("SPIFFS mounted");
    json_file = SPIFFS.open(SETTINGS_FNAME, "r");
    if (!json_file.available())
    {
        set_ledcolor(J_COLORRGB("ledcolor_error"));
        vfd_set("json err 1");
        Serial.println("data/settings.json file not found. Upload to SPIFFS");
        return;
    }
    DeserializationError err = deserializeJson(j_settings, json_file);
    if (err)
    {
        Serial.print(F("settings.json deserialization error: "));
        Serial.println(err.c_str());
        set_ledcolor(J_COLORRGB("ledcolor_error"));
        vfd_set("json err 2");
        return;
    }
    json_file.close();
    neopixel.fill(J_COLORRGB("ledcolor_wait"));
    neopixel.show();
    items_max_index = j_settings["queries"][0]["get"].size() - 1;
    selections_max_index = j_settings["queries"].size(); // Including "off"
    wificonnection = WiFiConnectionHandler(j_settings["ssid"], j_settings["password"]);
    wificonnection.addCallback(NetworkConnectionEvent::DISCONNECTED, []()
                               { on_wlan_disconnected(); });
    wificonnection.addCallback(NetworkConnectionEvent::ERROR, []()
                               { on_wlan_error(); });
    wificonnection.addCallback(NetworkConnectionEvent::CONNECTED, []()
                               { on_wlan_connected(); });

    int cnt;
    // vfd_set("0123456789abcdefghijklmnopqrstuvwxyz");
    knob_clk.begin();
    knob_clk.onPressed(on_knob_turned);
    knob_sw.begin();
    knob_sw.onPressed(on_knob_released);
    knob_sw.onSequence(2, 1000, on_knob_dblclick);
}

NetworkConnectionState check;
void loop()
{
    static int t = millis();
    knob_clk.read();
    knob_dt.read();
    knob_sw.read();

    wificonnection.check();
}
