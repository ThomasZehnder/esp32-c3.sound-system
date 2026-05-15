#include "app_webserver.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "dfplayer.h"
#include "ir_sensor.h"
#include "ultrasound_pwm.h"

namespace
{
WebServer server(80);
bool *filesystemMounted = nullptr;
bool *wifiConnected = nullptr;
String *selectedSound = nullptr;
SoundTriggerCallback soundTrigger = nullptr;

void logRequest(const String &path)
{
    Serial.print("HTTP request: ");
    Serial.println(path);
}

void setAllowCors()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
}

String getContentType(const String &path)
{
    if (path.endsWith(".html"))
        return "text/html";
    if (path.endsWith(".css"))
        return "text/css";
    if (path.endsWith(".js"))
        return "application/javascript";
    if (path.endsWith(".json"))
        return "application/json";
    if (path.endsWith(".png"))
        return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
        return "image/jpeg";
    if (path.endsWith(".gif"))
        return "image/gif";
    if (path.endsWith(".svg"))
        return "image/svg+xml";
    if (path.endsWith(".ico"))
        return "image/x-icon";
    return "text/plain";
}

bool handleFileRead(String path)
{
    logRequest(path);

    if (!filesystemMounted || !*filesystemMounted)
    {
        Serial.println("LittleFS not mounted; cannot serve file.");
        return false;
    }

    if (path.endsWith("/"))
    {
        path += "index.html";
    }

    if (!LittleFS.exists(path))
    {
        Serial.print("LittleFS file not found: ");
        Serial.println(path);
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file)
    {
        Serial.print("LittleFS open failed: ");
        Serial.println(path);
        return false;
    }

    const size_t fileSize = file.size();
    const String contentType = getContentType(path);
    Serial.print("Serving file: ");
    Serial.print(path);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    const size_t bytesSent = server.streamFile(file, contentType);
    file.close();

    Serial.print("Sent bytes: ");
    Serial.println(bytesSent);

    if (bytesSent != fileSize)
    {
        Serial.println("Stream incomplete.");
        return false;
    }

    return true;
}

void serveFileOr404(const String &path)
{
    if (!handleFileRead(path))
    {
        server.send(404, "text/plain", "404: " + path + " not found");
    }
}

void registerStaticRoute(const char *routePath)
{
    server.on(routePath, HTTP_GET, [routePath]()
              { serveFileOr404(String(routePath)); });
}

} // namespace

String getAssemblyJson()
{
    if (wifiConnected)
    {
        *wifiConnected = WiFi.status() == WL_CONNECTED;
    }

    const bool isWifiConnected = wifiConnected && *wifiConnected;
    String hostname = WiFi.getHostname() ? String(WiFi.getHostname()) : String("");
    String ipAddress = isWifiConnected ? WiFi.localIP().toString() : String("");
    String macAddress = WiFi.macAddress();
    String ssid = isWifiConnected ? WiFi.SSID() : String("");

    String output = "{";
    output += "\"device\":\"ESP32-C3 OLED\",";
    output += "\"hostname\":\"" + hostname + "\",";
    output += "\"chipModel\":\"" + String(ESP.getChipModel()) + "\",";
    output += "\"chipRevision\":" + String(ESP.getChipRevision()) + ",";
    output += "\"cpuFreqMHz\":" + String(ESP.getCpuFreqMHz()) + ",";
    output += "\"flashSize\":" + String(ESP.getFlashChipSize()) + ",";
    output += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    output += "\"sketchSize\":" + String(ESP.getSketchSize()) + ",";
    output += "\"freeSketchSpace\":" + String(ESP.getFreeSketchSpace()) + ",";
    output += "\"millis\":" + String(millis()) + ",";
    output += "\"wifiConnected\":" + String(isWifiConnected ? "true" : "false") + ",";
    output += "\"filesystemMounted\":" + String(filesystemMounted && *filesystemMounted ? "true" : "false") + ",";
    output += "\"selectedSound\":\"" + (selectedSound ? *selectedSound : String("none")) + "\",";
    output += "\"ssid\":\"" + ssid + "\",";
    output += "\"localIp\":\"" + ipAddress + "\",";
    output += "\"macAddress\":\"" + macAddress + "\",";
    output += "\"irDetected\":" + String(isIrDetected() ? "true" : "false") + ",";
    output += "\"irDetectedString\":\"" + String(getIrDetectedString()) + "\"";
    if (isWifiConnected)
    {
        output += ",\"rssi\":" + String(WiFi.RSSI());
    }

    const time_t now = time(nullptr);
    const bool ntpSynced = now > 1600000000UL;
    output += ",\"ntpSynced\":" + String(ntpSynced ? "true" : "false");
    if (ntpSynced)
    {
        struct tm utcInfo;
        struct tm localInfo;
        gmtime_r(&now, &utcInfo);
        localtime_r(&now, &localInfo);

        char utcStr[32];
        char localStr[32];
        strftime(utcStr, sizeof(utcStr), "%Y-%m-%dT%H:%M:%SZ", &utcInfo);
        strftime(localStr, sizeof(localStr), "%Y-%m-%dT%H:%M:%S", &localInfo);

        output += ",\"utcTime\":\"" + String(utcStr) + "\"";
        output += ",\"localTime\":\"" + String(localStr) + "\"";
        output += ",\"isDst\":" + String(localInfo.tm_isdst > 0 ? "true" : "false");
    }

    output += "}";
    return output;
}

namespace
{

void assemblyJson()
{
    setAllowCors();
    server.send(200, "application/json", getAssemblyJson());
}

void soundConfigJson()
{
    size_t soundCount = 0;
    const SoundDefinition *sounds = getSoundDefinitions(soundCount);
    size_t sequenceCount = 0;
    const SoundSequenceStep *sequence = getSoundSequence(sequenceCount);

    String output = "{";
    output += "\"sounds\":[";

    for (size_t index = 0; index < soundCount; ++index)
    {
        const SoundDefinition &sound = sounds[index];
        if (index > 0)
        {
            output += ",";
        }

        output += "{";
        output += "\"tag\":\"" + String(sound.tag) + "\",";
        output += "\"buttonLabel\":\"" + String(sound.buttonLabel) + "\",";
        output += "\"description\":\"" + String(sound.description) + "\",";
        output += "\"folder\":" + String(sound.folder) + ",";
        output += "\"track\":" + String(sound.track);
        output += "}";
    }

    output += "],";
    output += "\"sequence\":[";

    for (size_t index = 0; index < sequenceCount; ++index)
    {
        const SoundSequenceStep &step = sequence[index];
        if (index > 0)
        {
            output += ",";
        }

        output += "{";
        output += "\"kind\":\"" + String(step.kind) + "\",";
        output += "\"tag\":\"" + String(step.tag) + "\",";
        output += "\"durationMs\":" + String(step.durationMs) + ",";
        output += "\"description\":\"" + String(step.description) + "\"";
        output += "}";
    }

    output += "],";
    output += "\"selectedSound\":\"" + (selectedSound ? *selectedSound : String("")) + "\",";
    output += "\"sequenceRunning\":" + String(isSoundSequenceRunning() ? "true" : "false") + ",";
    output += "\"volume\":" + String(getDfPlayerVolume()) + ",";
    output += "\"volumePercent\":" + String(getGlobalVolumePercent()) + ",";
    output += "\"ultrasoundVolumePercent\":" + String(getNormalizedUltrasoundVolumePercent()) + ",";
    output += "\"volumeMin\":" + String(getDfPlayerMinVolume()) + ",";
    output += "\"volumeMax\":" + String(getDfPlayerMaxVolume());
    output += "}";

    setAllowCors();
    server.send(200, "application/json", output);
}

void sequenceConfigJson()
{
    size_t soundCount = 0;
    const SoundDefinition *sounds = getSoundDefinitions(soundCount);
    size_t ultrasoundCount = 0;
    const UltrasoundSequenceDefinition *ultrasounds = getUltrasoundSequenceDefinitions(ultrasoundCount);
    size_t sequenceCount = 0;
    const SoundSequenceStep *sequence = getSoundSequence(sequenceCount);
    const SoundSequenceSettings sequenceSettings = getSoundSequenceSettings();

    String output = "{";
    output += "\"maxSteps\":" + String(getMaxSoundSequenceSteps()) + ",";
    output += "\"sequenceSource\":\"" + String(isSoundSequenceLoadedFromFilesystem() ? "littlefs" : "default") + "\",";
    output += "\"sounds\":[";

    for (size_t index = 0; index < soundCount; ++index)
    {
        const SoundDefinition &sound = sounds[index];
        if (index > 0)
        {
            output += ",";
        }

        output += "{";
        output += "\"tag\":\"" + String(sound.tag) + "\",";
        output += "\"buttonLabel\":\"" + String(sound.buttonLabel) + "\",";
        output += "\"description\":\"" + String(sound.description) + "\"";
        output += "}";
    }

    output += "],";
    output += "\"ultrasounds\":[";

    for (size_t index = 0; index < ultrasoundCount; ++index)
    {
        const UltrasoundSequenceDefinition &ultrasound = ultrasounds[index];
        if (index > 0)
        {
            output += ",";
        }

        output += "{";
        output += "\"tag\":\"" + String(ultrasound.tag) + "\",";
        output += "\"buttonLabel\":\"" + String(ultrasound.buttonLabel) + "\",";
        output += "\"description\":\"" + String(ultrasound.description) + "\",";
        output += "\"minFrequencyHz\":" + String(ultrasound.minFrequencyHz) + ",";
        output += "\"maxFrequencyHz\":" + String(ultrasound.maxFrequencyHz) + ",";
        output += "\"randomMode\":" + String(ultrasound.randomMode ? "true" : "false");
        output += "}";
    }

    output += "],";
    output += "\"sequence\":[";

    for (size_t index = 0; index < sequenceCount; ++index)
    {
        const SoundSequenceStep &step = sequence[index];
        if (index > 0)
        {
            output += ",";
        }

        output += "{";
        output += "\"kind\":\"" + String(step.kind) + "\",";
        output += "\"tag\":\"" + String(step.tag) + "\",";
        output += "\"durationMs\":" + String(step.durationMs);
        output += "}";
    }

    output += "],";
    output += "\"startVolumePercent\":" + String(sequenceSettings.startVolumePercent) + ",";
    output += "\"currentVolumePercent\":" + String(getGlobalVolumePercent()) + "}";

    setAllowCors();
    server.send(200, "application/json", output);
}

void saveSequenceConfig()
{
    if (!server.hasArg("plain"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing body\"}");
        return;
    }

    JsonDocument document;
    DeserializationError error = deserializeJson(document, server.arg("plain"));
    if (error)
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    JsonArray steps = document["sequence"].as<JsonArray>();
    if (steps.isNull())
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing sequence\"}");
        return;
    }

    const int startVolumePercent = document["startVolumePercent"] | getSoundSequenceSettings().startVolumePercent;
    if (startVolumePercent < 0 || startVolumePercent > 100)
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"start volume out of range\"}");
        return;
    }

    const size_t stepCount = steps.size();
    if (stepCount == 0 || stepCount > getMaxSoundSequenceSteps())
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid step count\"}");
        return;
    }

    SoundSequenceStep tempSteps[MAX_SOUND_SEQUENCE_STEPS];
    char tempKinds[MAX_SOUND_SEQUENCE_STEPS][16] = {};
    char tempTags[MAX_SOUND_SEQUENCE_STEPS][24] = {};
    char tempDescriptions[MAX_SOUND_SEQUENCE_STEPS][80] = {};

    size_t index = 0;
    for (JsonObject step : steps)
    {
        const char *kind = step["kind"] | "";
        const char *tag = step["tag"] | "";
        const uint32_t durationMs = step["durationMs"] | 0;

        snprintf(tempKinds[index], sizeof(tempKinds[index]), "%s", kind);
        snprintf(tempTags[index], sizeof(tempTags[index]), "%s", tag);
        snprintf(tempDescriptions[index], sizeof(tempDescriptions[index]), "%s", strcmp(kind, "pause") == 0 ? "Pause step" : tag);

        tempSteps[index].kind = tempKinds[index];
        tempSteps[index].tag = tempTags[index];
        tempSteps[index].durationMs = durationMs;
        tempSteps[index].description = tempDescriptions[index];
        ++index;
    }

    const bool ok = setSoundSequence(tempSteps, stepCount) && setSoundSequenceSettings({static_cast<uint8_t>(startVolumePercent)});
    const bool persisted = ok ? saveSoundSequenceToFilesystem() : false;
    String output = "{";
    output += "\"ok\":" + String(ok ? "true" : "false") + ",";
    output += "\"count\":" + String(stepCount) + ",";
    output += "\"startVolumePercent\":" + String(startVolumePercent) + ",";
    output += "\"persisted\":" + String(persisted ? "true" : "false");
    output += "}";

    setAllowCors();
    server.send(ok && persisted ? 200 : (ok ? 503 : 400), "application/json", output);
}

void setVolume()
{
    if (!server.hasArg("value"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
        return;
    }

    const int requestedValue = server.arg("value").toInt();
    if (requestedValue < 0 || requestedValue > 100)
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"volume out of range\"}");
        return;
    }

    const bool ok = setGlobalVolumePercent(static_cast<uint8_t>(requestedValue));

    String output = "{";
    output += "\"ok\":" + String(ok ? "true" : "false") + ",";
    output += "\"volume\":" + String(getDfPlayerVolume()) + ",";
    output += "\"volumePercent\":" + String(getGlobalVolumePercent()) + ",";
    output += "\"ultrasoundVolumePercent\":" + String(getNormalizedUltrasoundVolumePercent());
    output += "}";

    setAllowCors();
    server.send(ok ? 200 : 503, "application/json", output);
}

void sequenceStateJson()
{
    size_t sequenceCount = 0;
    const SoundSequenceStep *sequence = getSoundSequence(sequenceCount);
    const int currentIndex = getSoundSequenceCurrentIndex();

    String output = "{";
    output += "\"sequenceRunning\":" + String(isSoundSequenceRunning() ? "true" : "false") + ",";
    output += "\"stepActive\":" + String(isSoundSequenceStepActive() ? "true" : "false") + ",";
    output += "\"currentIndex\":" + String(currentIndex) + ",";
    output += "\"elapsedMs\":" + String(getSoundSequenceElapsedMs()) + ",";
    output += "\"remainingMs\":" + String(getSoundSequenceRemainingMs()) + ",";
    output += "\"selectedSound\":\"" + (selectedSound ? *selectedSound : String("")) + "\"";

    if (currentIndex >= 0 && static_cast<size_t>(currentIndex) < sequenceCount)
    {
        const SoundSequenceStep &step = sequence[currentIndex];
        output += ",\"currentStep\":{";
        output += "\"kind\":\"" + String(step.kind) + "\",";
        output += "\"tag\":\"" + String(step.tag) + "\",";
        output += "\"durationMs\":" + String(step.durationMs) + ",";
        output += "\"description\":\"" + String(step.description) + "\"";
        output += "}";
    }

    output += "}";

    setAllowCors();
    server.send(200, "application/json", output);
}

void setSequence()
{
    if (!server.hasArg("action"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing action\"}");
        return;
    }

    const String action = server.arg("action");
    bool ok = false;

    if (action == "start")
    {
        ok = startSoundSequence(SequenceMode::LOOP);
        if (ok && selectedSound)
        {
            Serial.println("[WEB] Start Sequence in LOOP");
            *selectedSound = "sequence";
        }
    }
    else if (action == "stop")
    {
        stopSoundSequenceByUser();
        ok = true;
        Serial.println("[WEB] Stop Sequence");
        if (selectedSound)
        {
            *selectedSound = "stop";
        }
    }
    else
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid action\"}");
        return;
    }

    String output = "{";
    output += "\"ok\":" + String(ok ? "true" : "false") + ",";
    output += "\"sequenceRunning\":" + String(isSoundSequenceRunning() ? "true" : "false") + ",";
    output += "\"selectedSound\":\"" + (selectedSound ? *selectedSound : String("")) + "\"";
    output += "}";

    setAllowCors();
    server.send(ok ? 200 : 503, "application/json", output);
}

void setSound()
{
    if (!server.hasArg("name"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing name\"}");
        return;
    }

    String soundName = server.arg("name");
    bool started = false;

    if (selectedSound)
    {
        *selectedSound = soundName;
        Serial.print("Selected sound: ");
        Serial.println(*selectedSound);
    }

    if (soundTrigger)
    {
        started = soundTrigger(soundName);
    }

    String output = "{";
    output += "\"ok\":" + String(started ? "true" : "false") + ",";
    output += "\"selectedSound\":\"" + (selectedSound ? *selectedSound : String("")) + "\"";
    output += "}";

    setAllowCors();
    server.send(200, "application/json", output);
}

void ultrasoundStateJson()
{
    String output = "{";
    output += "\"ok\":true,";
    output += "\"pin\":" + String(getUltrasoundPin()) + ",";
    output += "\"randomMode\":" + String(isUltrasoundRandomMode() ? "true" : "false") + ",";
    output += "\"frequencyHz\":" + String(getUltrasoundFrequencyHz()) + ",";
    output += "\"randomMinFrequencyHz\":" + String(getUltrasoundRandomMinFrequencyHz()) + ",";
    output += "\"randomMaxFrequencyHz\":" + String(getUltrasoundRandomMaxFrequencyHz()) + ",";
    output += "\"volumePercent\":" + String(getUltrasoundVolumePercent()) + ",";
    output += "\"remainingMs\":" + String(getUltrasoundRemainingMs()) + ",";
    output += "\"playing\":" + String(isUltrasoundPlaying() ? "true" : "false") + ",";
    output += "\"frequencyMinHz\":" + String(getUltrasoundMinFrequencyHz()) + ",";
    output += "\"frequencyMaxHz\":" + String(getUltrasoundMaxFrequencyHz());
    output += "}";

    setAllowCors();
    server.send(200, "application/json", output);
}

void setUltrasound()
{
    if (!server.hasArg("action"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing action\"}");
        return;
    }

    const String action = server.arg("action");
    if (action == "stop")
    {
        stopUltrasound();

        String output = "{";
        output += "\"ok\":true,";
        output += "\"playing\":false,";
        output += "\"message\":\"ultrasound stopped\"";
        output += "}";

        setAllowCors();
        server.send(200, "application/json", output);
        return;
    }

    if (action != "start" && action != "random")
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid action\"}");
        return;
    }

    if (!server.hasArg("duration"))
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing parameters\"}");
        return;
    }

    const long durationMs = server.arg("duration").toInt();
    const uint8_t volumePercent = getNormalizedUltrasoundVolumePercent();

    if (durationMs <= 0)
    {
        setAllowCors();
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"duration must be positive\"}");
        return;
    }

    bool ok = false;

    if (action == "start")
    {
        if (!server.hasArg("frequency"))
        {
            setAllowCors();
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing frequency\"}");
            return;
        }

        const long frequencyHz = server.arg("frequency").toInt();
        if (frequencyHz < static_cast<long>(getUltrasoundMinFrequencyHz()) || frequencyHz > static_cast<long>(getUltrasoundMaxFrequencyHz()))
        {
            setAllowCors();
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"frequency out of range\"}");
            return;
        }

        ok = playUltrasound(static_cast<uint32_t>(frequencyHz), static_cast<uint8_t>(volumePercent), static_cast<uint32_t>(durationMs));
    }
    else
    {
        if (!server.hasArg("minFrequency") || !server.hasArg("maxFrequency"))
        {
            setAllowCors();
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing random frequency range\"}");
            return;
        }

        const long minFrequencyHz = server.arg("minFrequency").toInt();
        const long maxFrequencyHz = server.arg("maxFrequency").toInt();
        if (minFrequencyHz < static_cast<long>(getUltrasoundMinFrequencyHz()) || minFrequencyHz > static_cast<long>(getUltrasoundMaxFrequencyHz()) || maxFrequencyHz < static_cast<long>(getUltrasoundMinFrequencyHz()) || maxFrequencyHz > static_cast<long>(getUltrasoundMaxFrequencyHz()))
        {
            setAllowCors();
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"random frequency range out of range\"}");
            return;
        }

        if (minFrequencyHz > maxFrequencyHz)
        {
            setAllowCors();
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"min frequency must be <= max frequency\"}");
            return;
        }

        ok = playRandomUltrasound(static_cast<uint32_t>(minFrequencyHz), static_cast<uint32_t>(maxFrequencyHz), static_cast<uint8_t>(volumePercent), static_cast<uint32_t>(durationMs));
    }

    String output = "{";
    output += "\"ok\":" + String(ok ? "true" : "false") + ",";
    output += "\"playing\":" + String(isUltrasoundPlaying() ? "true" : "false") + ",";
    output += "\"randomMode\":" + String(isUltrasoundRandomMode() ? "true" : "false") + ",";
    output += "\"frequencyHz\":" + String(getUltrasoundFrequencyHz()) + ",";
    output += "\"randomMinFrequencyHz\":" + String(getUltrasoundRandomMinFrequencyHz()) + ",";
    output += "\"randomMaxFrequencyHz\":" + String(getUltrasoundRandomMaxFrequencyHz()) + ",";
    output += "\"volumePercent\":" + String(getUltrasoundVolumePercent()) + ",";
    output += "\"remainingMs\":" + String(getUltrasoundRemainingMs());
    output += "}";

    setAllowCors();
    server.send(ok ? 200 : 503, "application/json", output);
}
}

void setupWebServer(bool *filesystemMountedState, bool *wifiConnectedState, String *selectedSoundState, SoundTriggerCallback soundTriggerCallback)
{
    filesystemMounted = filesystemMountedState;
    wifiConnected = wifiConnectedState;
    selectedSound = selectedSoundState;
    soundTrigger = soundTriggerCallback;

    server.on("/", HTTP_GET, []()
              { serveFileOr404("/index.html"); });

    registerStaticRoute("/index.html");
    registerStaticRoute("/scripts.js");
    registerStaticRoute("/style.css");
    registerStaticRoute("/favicon.svg");
    registerStaticRoute("/a-home.html");
    registerStaticRoute("/a-sounds.html");
    registerStaticRoute("/a-ultrasound.html");
    registerStaticRoute("/a-config.html");
    registerStaticRoute("/a-sequence.html");

    server.on("/assembly", HTTP_GET, assemblyJson);
    server.on("/sound-config", HTTP_GET, soundConfigJson);
    server.on("/sequence-config", HTTP_GET, sequenceConfigJson);
    server.on("/sequence-config", HTTP_POST, saveSequenceConfig);
    server.on("/sequence-state", HTTP_GET, sequenceStateJson);
    server.on("/sound", HTTP_GET, setSound);
    server.on("/sequence", HTTP_GET, setSequence);
    server.on("/volume", HTTP_GET, setVolume);
    server.on("/ultrasound", HTTP_GET, setUltrasound);
    server.on("/ultrasound-state", HTTP_GET, ultrasoundStateJson);

    server.on("/inline", []()
              { server.send(200, "text/plain", "this works as well"); });

    server.onNotFound([]()
                      {
                          if (!handleFileRead(server.uri()))
                          {
                              server.send(404, "text/plain", "404: Not Found");
                          } });

    server.begin();
    Serial.println("HTTP server started.");
}

void handleWebServerClient()
{
    server.handleClient();
}