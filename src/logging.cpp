#include <Arduino.h>
#include <SDFS.h>

#include "customizations.h"

#define DEBUG_LOG_FILENAME "LOG.TXT"

void log_print(const char *message) {
    Serial.print(message);

#if DEBUG_LOG_ENABLED
    File file = SDFS.open(DEBUG_LOG_FILENAME, "a");
    file.print(message);
    file.close();
#endif
}

void log_printf(const char *message, ...) {
    va_list argp;
    va_start(argp, message);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    log_print(buffer);
}

void log_vprintf(const char *message, va_list argp) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    log_print(buffer);
}

void log_printfln(const char *message, ...) {
    va_list argp;
    va_start(argp, message);
    log_vprintf(message, argp);
    log_print("\n");
}

void log_println(String message) {
    log_print(message.c_str());
    log_print("\n");
}
