#include <array>
#include <functional>

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "ledcontroller.h"

constexpr auto WIFI_SSID = "McDonalds Free WiFi 2.4GHz";
constexpr auto WIFI_PASSWD = "Passwort_123";

LedController ledController;

ESP8266WebServer server(80);

bool power = true;
bool rotatePattern = true;

void setup()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    server.on("/", HTTP_GET, []()
    {
        server.sendHeader("Connection", "close");

        server.send(200, "text/html",
                    "<!doctype html>"
                    "<html lang=\"en\">"
                        "<head>"
                            "<!-- Required meta tags -->"
                            "<meta charset=\"utf-8\">"
                            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">"

                            "<!-- Bootstrap CSS -->"
                            "<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" integrity=\"sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T\" crossorigin=\"anonymous\">"

                            "<style>"
                                ".hidden {"
                                    "display: none;"
                                "}"
                            "</style>"

                            "<title>Hello, world!</title>"
                        "</head>"
                        "<body>"
                            "<div class=\"container\">"
                                "<h1>Hello, world!</h1>"
                                "<p>"
                                    "<a href=\"setPower?val=true\" id=\"enablePower\" class=\"softLink\">Turn LEDs on</a> "
                                    "<a href=\"setPower?val=false\" id=\"disablePower\" class=\"softLink\">Turn LEDs off</a> "
                                    "<a href=\"nextPattern\" id=\"nextPattern\" class=\"softLink\">Next pattern</a> "
                                    "<a href=\"setPatternRotate?val=true\" id=\"enableRotate\" class=\"softLink\">Enable automatic pattern rotate</a> "
                                    "<a href=\"setPatternRotate?val=false\" id=\"disableRotate\" class=\"softLink\">Disable automatic pattern rotate</a>"
                                "</p>"

                                "<select id=\"patternSelect\"></select>"

                                "<input type=\"range\" min=\"0\" max=\"255\" id=\"brightness\" />"
                            "</div>"

                            "<script src=\"https://code.jquery.com/jquery-3.3.1.slim.min.js\" integrity=\"sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo\" crossorigin=\"anonymous\"></script>"
                            "<script>"
                                "jQuery(document).ready(function($){"
                                    "$('.softLink').click(function(){"
                                        "var xhr = new XMLHttpRequest();"
                                        "xhr.open('GET', this.getAttribute('href'));"
                                        "xhr.send();"

                                        "return false;"
                                    "});"

                                    "$('#patternSelect').change(function(){"
                                        "var xhr = new XMLHttpRequest();"
                                        "xhr.open('GET', 'setPattern?val=' + this.selectedIndex);"
                                        "xhr.send();"
                                    "});"

                                    "$('#brightness').change(function(){"
                                        "var xhr = new XMLHttpRequest();"
                                        "xhr.open('GET', 'setBrightness?val=' + this.value);"
                                        "xhr.send();"
                                    "});"

                                    "var refreshStatus = function() {"
                                        "var xhr = new XMLHttpRequest();"
                                        "xhr.open('GET', 'status');"
                                        "xhr.onload = function() {"
                                            "var parsed = JSON.parse(xhr.responseText);"

                                            "function setHidden(element, hidden) {"
                                                "if (hidden) {"
                                                    "element.addClass('hidden');"
                                                "} else {"
                                                    "element.removeClass('hidden');"
                                                "}"
                                            "}"

                                            "setHidden($('#enablePower'), parsed.power);"
                                            "setHidden($('#disablePower'), !parsed.power);"
                                            "setHidden($('#nextPattern'), !parsed.power);"
                                            "setHidden($('#enableRotate'), !parsed.power || parsed.autoRotate);"
                                            "setHidden($('#disableRotate'), !parsed.power || !parsed.autoRotate);"
                                            "setHidden($('#patternSelect'), !parsed.power);"
                                            "setHidden($('#brightness'), !parsed.power);"

                                            "var select = $('#patternSelect');"
                                            "select.empty();"
                                            "select.attr('size', parsed.patterns.length);"

                                            "for (var pattern of parsed.patterns) {"
                                                "select.append($('<option>')"
                                                    ".text(pattern.name)"
                                                    ".attr('selected', pattern.selected)"
                                                ");"
                                            "}"

                                            "$('#brightness').val(parsed.brightness)"
                                        "};"
                                        "xhr.send();"
                                    "};"

                                    "refreshStatus();"

                                    "setInterval(refreshStatus, 1000);"
                                "});"
                            "</script>"
                        "</body>"
                    "</html>"
                    );
    });

    server.on("/status", HTTP_GET, []()
    {
        server.sendHeader("Connection", "close");
        server.sendHeader("Access-Control-Allow-Origin", "*");

        String str = "{";

        str += "\"patterns\":[";
        bool first = true;
        for (auto pattern : ledController.patterns)
        {
            if (first)
                first = false;
            else
                str += ',';

            str += "{\"name\":\"";
            str += pattern->name();
            str += "\",\"selected\":";
            str += (*ledController.iter) == pattern ? "true" : "false";
            str += '}';
        }
        str += "],";

        str += "\"power\":";
        str += power ? "true" : "false";
        str += ",";

        str += "\"autoRotate\":";
        str += rotatePattern ? "true" : "false";
        str += ",";

        str += "\"brightness\":";
        str += String(int(FastLED.getBrightness()));

        str += "}";

        server.send(200, "text/json", str.c_str());
    });

    server.on("/nextPattern", HTTP_GET, []()
    {
        ledController.nextPattern();
        server.send(200, "text/html", "ok");
    });

    server.on("/setPattern", HTTP_GET, []()
    {
        const String *val = nullptr;

        for (int i = 0; i < server.args(); i++)
            if (server.argName(i) == "val")
                val = &server.arg(i);

        if (!val)
        {
            server.send(400, "text/html", "val missing");
            return;
        }

        const auto index = atoi(val->c_str());
        if (index < 0 || index >= ledController.patterns.size())
        {
            server.send(400, "text/html", "out of range");
            return;
        }

        ledController.iter = ledController.patterns.begin() + index;

        server.send(200, "text/html", "ok");
    });

    server.on("/setPower", HTTP_GET, []()
    {
        const String *val = nullptr;

        for (int i = 0; i < server.args(); i++)
            if (server.argName(i) == "val")
                val = &server.arg(i);

        if (!val)
        {
            server.send(400, "text/html", "val missing");
            return;
        }

        if (*val != "true" && *val != "false")
        {
            server.send(400, "text/html", "invalid val");
            return;
        }

        power = *val == "true";

        server.send(200, "text/html", "ok");
    });

    server.on("/setPatternRotate", HTTP_GET, []()
    {
        const String *val = nullptr;

        for (int i = 0; i < server.args(); i++)
            if (server.argName(i) == "val")
                val = &server.arg(i);

        if (!val)
        {
            server.send(400, "text/html", "val missing");
            return;
        }

        if (*val != "true" && *val != "false")
        {
            server.send(400, "text/html", "invalid val");
            return;
        }

        rotatePattern = *val == "true";

        server.send(200, "text/html", "ok");
    });

    server.on("/setBrightness", HTTP_GET, []()
    {
        const String *val = nullptr;

        for (int i = 0; i < server.args(); i++)
            if (server.argName(i) == "val")
                val = &server.arg(i);

        if (!val)
        {
            server.send(400, "text/html", "val missing");
            return;
        }

        const auto brightness = atoi(val->c_str());
        if (brightness < 0 || brightness > 255)
        {
            server.send(400, "text/html", "out of range");
            return;
        }

        FastLED.setBrightness(brightness);

        server.send(200, "text/html", "ok");
    });

    server.begin();
}
  
void loop()
{
    EVERY_N_MILLISECONDS(20)
    {
        if (power)
            ledController.run();
        else
            ledController.poweroff();
        FastLED.show();
    }

    if (power && rotatePattern)
    {
        EVERY_N_SECONDS(10)
        {
            ledController.nextPattern();
        }
    }

    server.handleClient();
}

