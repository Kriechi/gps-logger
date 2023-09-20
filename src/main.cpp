#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <SDFS.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Ticker.h>
#include <TinyGPS++.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "customizations.h"
#include "logging.h"
#include "wifi_credentials.h"

#define OWNER_FILENAME "OWNER.TXT"
#define LAST_KNOWN_LOCATION "LAST_KNOWN_LOCATION.TXT"
#define GPS_FILENAME "GPS.TXT"
#define GPS_COLUMN_HEADERS "timestamp;latitude;longitude;altitude;speed;number of satellites"
#define GPS_LOG_INTERVAL 5

#define TIMESTAMP_FORMAT "%04d-%02d-%02dT%02d:%02d:%02dZ"
#define TIMESTAMP_ARGS gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second()

#define ASSISTNOW_ONLINE_BASE_URL "http://online-live1.services.u-blox.com/GetOnlineData.ashx"
#define ASSISTNOW_OFFLINE_BASE_URL "http://offline-live1.services.u-blox.com/GetOfflineData.ashx"

void led_on() {
    digitalWrite(LED_BUILTIN, LOW);
}

void led_off() {
    digitalWrite(LED_BUILTIN, HIGH);
}

void setup();

void init_gps_file();
void upload_gps_file();
void init_assistnow();
bool download_online_blob(time_t epoch_time, struct tm *time_info);
bool download_offline_blob(time_t epoch_time, struct tm *time_info);
void load_assistnow_blob(String filename);
bool connect_wifi();
void check_serial_commands();
void persist_location_record();

int no_gps_lock_counter = 0;
int no_location_update_counter = 0;
bool gps_debug = false;
bool blinker_state = false;
bool first_location_record = true;
bool assistnow_initialized = false;
Ticker output_ticker;
SoftwareSerial uart_gps;
TinyGPSPlus gps;
double old_lat;
double old_lon;

double last_known_lat = ASSISTNOW_START_LAT;
double last_known_lon = ASSISTNOW_START_LON;
double last_known_alt = ASSISTNOW_START_ALT;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    led_off();

    Serial.begin(115200);
    Serial.print("\n\n. . . BOOTING ESP8266 . . .\n\n");

    SDFS.setConfig(SDFSConfig(D8, SPI_HALF_SPEED));
    if (!SDFS.begin()) {
        Serial.println("initialization failed!");
        return;
    }

    log_println("--------------------------------------------------------------------------------");
    log_printfln("BUILD: %s %s", __DATE__, __TIME__);
    log_println("Booted and SD initialized. Ready!");

    if (!SDFS.exists(OWNER_FILENAME)) {
        File owner_file = SDFS.open(OWNER_FILENAME, "w");
        owner_file.print(OWNER_CONTENT);
        owner_file.close();
        log_println("OWNER file written.");
    } else {
        log_println("OWNER file present.");
    }

    if (SDFS.exists(LAST_KNOWN_LOCATION)) {
        log_println("LAST_KNOWN_LOCATION file present.");
        File last_known_location_file = SDFS.open(LAST_KNOWN_LOCATION, "r");
        last_known_lat = last_known_location_file.readStringUntil('\n').toDouble();
        last_known_lon = last_known_location_file.readStringUntil('\n').toDouble();
        last_known_alt = last_known_location_file.readStringUntil('\n').toDouble();
        last_known_location_file.close();
        log_printfln("Last known location: lat:%.6f, lon:%.6f, alt:%.6f", last_known_lat, last_known_lon, last_known_alt);
    }

    if (SDFS.exists(GPS_FILENAME)) {
        File gpsFile = SDFS.open(GPS_FILENAME, "r");
        if (gpsFile.size() > strlen(GPS_COLUMN_HEADERS) + 2) {
            log_printfln("GPS file with data present with %d bytes.", gpsFile.size());
            upload_gps_file();
        } else if (gpsFile.size() == strlen(GPS_COLUMN_HEADERS) + 2) {
            log_println("GPS file present and without data.");
        } else {
            log_println("GPS file smaller than expected - deleting it.");
            SDFS.remove(GPS_FILENAME);
        }
    }

    init_gps_file();

    uart_gps.begin(9600, SWSERIAL_8N1, D1, D2);
    log_println("GPS module connection started.");

    output_ticker.attach(GPS_LOG_INTERVAL, persist_location_record);
}

void init_gps_file() {
    if (SDFS.exists(GPS_FILENAME)) {
        return;
    }
    File gpsFile = SDFS.open(GPS_FILENAME, "w");
    gpsFile.println(GPS_COLUMN_HEADERS);
    gpsFile.close();
    log_println("GPS file initialized.");
}

bool connect_wifi() {
    const char *found_ssid = NULL;
    int n = 0;
    for (int i = 0; i < 3; i++) {
        n = WiFi.scanNetworks();
        if (n > 0) {
            break;
        }
        delay(250);
    }

    for (int i = 0; i < n; ++i) {
        int j = 0;
        while (WIFI_CREDENTIALS[j][0] != NULL) {
            if (WiFi.SSID(i) == WIFI_CREDENTIALS[j][0]) {
                found_ssid = WIFI_CREDENTIALS[j][0];
                const char *passphrase = WIFI_CREDENTIALS[j][1];
                WiFi.begin(found_ssid, passphrase);
                break;
            }
            j++;
        }
    }

    if (found_ssid == NULL) {
        log_println("No known WiFi found.");
        return false;
    }

    log_printfln("Connecting to WiFi: %s ...", found_ssid);
    Serial.flush();

    int tries = 50;
    while (WiFi.status() != WL_CONNECTED && tries > 0) {
        delay(250);
        tries--;
    }
    if (tries == 0) {
        log_println("Failed to connect to WiFi!");
        return false;
    }
    WiFi.mode(WIFI_STA);

    log_print("Received IP: ");
    log_println(WiFi.localIP().toString());
    Serial.flush();

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    delay(2000);
    log_printfln("NTP epoch: %ld", time(nullptr));

    return true;
}

void loop() {
    while (uart_gps.available() > 0) {
        check_serial_commands();

        int b = uart_gps.read();
        if (gps_debug) {
            Serial.printf("%c", b);
        }
        gps.encode(b);

        if (!assistnow_initialized) {
            init_assistnow();
        }

        ESP.wdtFeed();
        yield();
    }
}

void check_serial_commands() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "g" || command == "gps") {
            gps_debug = !gps_debug;
            log_printfln("Toggling GPS debug %s...", gps_debug ? "on" : "off");
        } else if (command == "r" || command == "reset") {
            log_println("Restarting ESP...");
            ESP.restart();
        } else if (command == "u" || command == "upload") {
            log_println("Executing upload routine...");
            upload_gps_file();
        }
    }
}

void persist_location_record() {
    static size_t tick_counter = 0;
    tick_counter++;

    if (blinker_state) {
        blinker_state = false;
        led_off();
    } else {
        blinker_state = true;
        led_on();
    }

    if (!gps.location.isValid()) {
        if (no_gps_lock_counter++ % int(300.0 / GPS_LOG_INTERVAL) == 0) {
            led_off();
            log_printfln(TIMESTAMP_FORMAT ": No GPS lock: %u satellites.",
                         TIMESTAMP_ARGS,
                         gps.satellites.value());
        }
        return;
    }
    no_gps_lock_counter = 0;

    if (!gps.location.isUpdated()) {
        if (no_location_update_counter++ % int(300.0 / GPS_LOG_INTERVAL) == 0) {
            log_printfln(TIMESTAMP_FORMAT ": Last GPS location update was %.0f seconds ago.",
                         TIMESTAMP_ARGS,
                         gps.location.age() / 1000.0);
        }
        return;
    }
    no_location_update_counter = 0;

    double new_lat = gps.location.lat();
    double new_lon = gps.location.lng();
    double distance_travelled = gps.distanceBetween(old_lat, old_lon, new_lat, new_lon);
    if (distance_travelled < 5.0) {
        log_printfln(TIMESTAMP_FORMAT ": Only travelled %.2f meters since last update - skipping.",
                     TIMESTAMP_ARGS,
                     distance_travelled);
        return;
    } else {
        old_lat = new_lat;
        old_lon = new_lon;
    }

    char record[80];
    snprintf(record, sizeof(record), TIMESTAMP_FORMAT ";%.6f;%.6f;%.2f;%.2f;%d",
             TIMESTAMP_ARGS,
             new_lat, new_lon, gps.altitude.meters(), gps.speed.mps(), gps.satellites.value());

    File gpsFile = SDFS.open(GPS_FILENAME, "a");
    gpsFile.println(record);
    gpsFile.close();

    if (tick_counter % (300 / GPS_LOG_INTERVAL) == 0) {
        File last_known_location_file = SDFS.open(LAST_KNOWN_LOCATION, "w");
        snprintf(record, sizeof(record), "%.6f", last_known_lat);
        last_known_location_file.println(record);
        snprintf(record, sizeof(record), "%.6f", last_known_lon);
        last_known_location_file.println(record);
        snprintf(record, sizeof(record), "%.6f", last_known_alt);
        last_known_location_file.println(record);
        last_known_location_file.close();
    }

    // do not use log_print helpers here, only log to Serial and the GPS file, but not to LOG file
    Serial.println(record);
}

void upload_gps_file() {
    bool connected = connect_wifi();
    if (!connected) {
        return;
    }

    log_printfln("Connecting to upload server at %s://%s:%d ...", (USE_SERVER_TLS ? "https" : "http"), UPLOAD_SERVER_HOST, UPLOAD_SERVER_PORT);

#if USE_SERVER_TLS
    WiFiClientSecure client;
    client.setFingerprint(UPLOAD_SERVER_TLS_FINGERPRINT);
    log_printfln("Set upload server HTTPS fingerprint: %s", UPLOAD_SERVER_TLS_FINGERPRINT);
#else
    WiFiClient client;
#endif
    if (!client.connect(UPLOAD_SERVER_HOST, UPLOAD_SERVER_PORT)) {
        log_printfln("Failed to connect to upload server!");
        return;
    }

    HTTPClient http;
    http.begin(client, UPLOAD_SERVER_HOST, UPLOAD_SERVER_PORT, UPLOAD_SERVER_PATH, USE_SERVER_TLS);

    log_printfln("Uploading GPS file to server...");

    http.useHTTP10();
    http.addHeader(UPLOAD_SERVER_MAGIC_HEADER, UPLOAD_SERVER_MAGIC_HEADER_VALUE);
    http.addHeader("Content-Type", "text/plain");

    File gpsFile = SDFS.open(GPS_FILENAME, "r");
    int http_code = http.sendRequest("POST", &gpsFile, gpsFile.size());
    gpsFile.close();
    client.stop();

    if (http_code == HTTP_CODE_OK) {
        log_println("GPS file successfully uploaded!");

        char newFilename[64];
        time_t t = time(nullptr);
        snprintf(newFilename, sizeof(newFilename), "GPS-%lld.TXT", t);
        SDFS.rename(GPS_FILENAME, newFilename);
        log_printfln("GPS file renamed to %s", newFilename);

        init_gps_file();
    } else {
        log_printfln("Upload failed with unexpected %d HTTP status code.", http_code);
    }

    return;
}

bool download_file_to_sd(char *url, char *filename) {
    log_printfln("AssistNow: downloading from %s into %s ...", url, filename);
    File file = SDFS.open(filename, "w");

    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);
    http.useHTTP10();
    int http_code = http.GET();
    if (http_code != HTTP_CODE_OK) {
        log_printfln("AssistNow: error downloading blob: %d", http_code);
        return false;
    }
    log_println("AssistNow: GET request successful. Downloading payload data...");

    bool error = false;
    uint8_t buf[128];
    int pos = 0;
    while (pos < http.getSize()) {
        size_t read_len = client.read(buf, sizeof(buf));
        if (read_len < 0) {
            log_println("AssistNow: error reading from HTTP");
            break;
        }

        size_t write_len = file.write(buf, read_len);
        if (write_len != read_len) {
            log_printfln("AssistNow: error writing to file: written only %d bytes out of %d in the read buffer.", write_len, read_len);
            error = true;
            break;
        }
        pos += read_len;
        yield();
    }
    file.flush();
    file.close();
    http.end();

    if (error) {
        SDFS.remove(filename);
        return false;
    }

    file = SDFS.open(filename, "r");
    log_printfln("AssistNow: download to SD card completed with %d bytes.", file.size());
    file.close();
    return true;
}

long epoch_from_filename(String filename) {
    if (!filename.startsWith("ASSISTNOW-")) {
        return 0;
    }
    filename.replace("ASSISTNOW-", "");
    filename.replace("ONLINE-", "");
    filename.replace("OFFLINE-", "");
    // extract the epoch portion, 2**32 is at most 10 digits long
    String file_epoch_str = filename.substring(0, 10);
    long file_epoch = atol(file_epoch_str.c_str());
    return file_epoch;
}

void cleanup_outdated_assistnow_blobs(time_t epoch_time) {
    fs::Dir dir = SDFS.openDir("/");
    while (dir.next()) {
        if (dir.fileName().startsWith("ASSISTNOW-") && dir.fileName().endsWith(".bin")) {
            int max_age = 0;
            if (dir.fileName().startsWith("ASSISTNOW-ONLINE-")) {
                max_age = 60 * 60 * 2;
            } else if (dir.fileName().startsWith("ASSISTNOW-OFFLINE-")) {
                max_age = 60 * 60 * 24;
            } else {
                SDFS.remove(dir.fileName());
                continue;
            }

            long file_epoch = epoch_from_filename(dir.fileName());
            if (epoch_time - file_epoch > max_age) {
                SDFS.remove(dir.fileName());
                log_printfln("AssistNow: deleted outdated blob: %s", dir.fileName().c_str());
            } else {
                log_printfln("AssistNow: keeping still valid blob: %s, %ld %ld %ld", dir.fileName().c_str(), epoch_time, file_epoch, max_age);
            }
        }
    }
}

String find_valid_assistnow_blob() {
    String best_match = "";
    time_t best_epoch = 0;

    fs::Dir dir = SDFS.openDir("/");
    while (dir.next()) {
        if (dir.fileName().startsWith("ASSISTNOW-ONLINE-")) {
            long file_epoch = epoch_from_filename(dir.fileName());
            if (file_epoch > best_epoch) {
                best_epoch = file_epoch;
                best_match = dir.fileName();
            }
        }
    }
    if (best_epoch > 0) {
        // prefer ONLINE blobs over OFFLINE blobs
        return best_match;
    }

    dir = SDFS.openDir("/");
    while (dir.next()) {
        if (dir.fileName().startsWith("ASSISTNOW-OFFLINE-")) {
            long file_epoch = epoch_from_filename(dir.fileName());
            if (file_epoch > best_epoch) {
                best_epoch = file_epoch;
                best_match = dir.fileName();
            }
        }
    }

    return best_match;
}

void init_assistnow() {
    assistnow_initialized = true;

    bool connected = connect_wifi();
    if (!connected) {
        return;
    }

    time_t epoch_time = time(nullptr);
    struct tm *time_info = gmtime(&epoch_time);

    cleanup_outdated_assistnow_blobs(epoch_time);

    // download OFFLINE data first
    download_offline_blob(epoch_time, time_info);
    // download ONLINE data and immediately upload it as the most recent blob
    download_online_blob(epoch_time, time_info);
    String filename = find_valid_assistnow_blob();
    load_assistnow_blob(filename);

    WiFi.disconnect(true);
}

bool download_online_blob(time_t epoch_time, struct tm *time_info) {
    char online_filename[60];
    snprintf(online_filename, sizeof(online_filename), "ASSISTNOW-ONLINE-%llu-%d-%02d-%02dT%02d-%02d-%02d.bin",
             epoch_time,
             time_info->tm_year + 1900,
             time_info->tm_mon + 1,
             time_info->tm_mday,
             time_info->tm_hour,
             time_info->tm_min,
             time_info->tm_sec);
    char url[200];
    snprintf(url, sizeof(url), "%s?token=%s;gnss=gps,glo,gal;datatype=eph,alm,aux,pos;lat=%.6f;lon=%.6f;alt=%.2f;pacc=50000;latency=1",
             ASSISTNOW_ONLINE_BASE_URL,
             ASSISTNOW_TOKEN,
             last_known_lat,
             last_known_lon,
             last_known_alt);
    return download_file_to_sd(url, online_filename);
}

bool download_offline_blob(time_t epoch_time, struct tm *time_info) {
    char offline_filename[60];
    snprintf(offline_filename, sizeof(offline_filename), "ASSISTNOW-OFFLINE-%llu-%d-%02d-%02dT%02d-%02d-%02d.bin",
             epoch_time,
             time_info->tm_year + 1900,
             time_info->tm_mon + 1,
             time_info->tm_mday,
             time_info->tm_hour,
             time_info->tm_min,
             time_info->tm_sec);

    // adding the almanac parameter causes an error, see https://portal.u-blox.com/s/question/0D52p00009in3mk/offline-assistnow-almanac-url
    char url[170];
    snprintf(url, sizeof(url), "%s?token=%s;gnss=gps,glo;format=mga;period=5;resolution=1",
             ASSISTNOW_OFFLINE_BASE_URL,
             ASSISTNOW_TOKEN);
    return download_file_to_sd(url, offline_filename);
}

void load_assistnow_blob(String filename) {
    if (filename.length() == 0) {
        return;
    }

    log_printfln("AssistNow: uploading blob %s to module...", filename.c_str());
    File file = SDFS.open(filename, "r");
    size_t size = file.size();
    if (size <= 0) {
        log_println("AssistNow: blob file with invalid size. Skipping loading it.");
        return;
    }

    uint8_t buf[128];
    size_t pos = 0;
    while (pos < size) {
        int read_len = file.read(buf, sizeof(buf));
        if (read_len < 0) {
            log_println("AssistNow: file read error");
            return;
        }
        uart_gps.write(buf, read_len);
        pos += read_len;
    }
    file.close();
    log_println("AssistNow: upload to module completed.");
}
