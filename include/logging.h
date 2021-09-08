#ifndef LOGGING_H
#define LOGGING_H

void log_print(const char *message);

void log_printf(const char *message, ...);

void log_vprintf(const char *message, va_list argp);

void log_printfln(const char *message, ...);

void log_println(String message);

#endif // LOGGING_H
