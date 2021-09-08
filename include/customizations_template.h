#define OWNER_CONTENT "\
John Doe\r\n\
123 Main St Anytown\r\n\
Anytown, PA, 17101\r\n\
+1 206 555 0100\r\n\
john.doe@example.com\r\n\
"

#define DEBUG_LOG_ENABLED 1

#define ASSISTNOW_TOKEN "123456789asdf"
#define ASSISTNOW_START_LAT 42.4
#define ASSISTNOW_START_LON -75.7
#define ASSISTNOW_START_ALT 0.0

#define USE_SERVER_TLS 1
#if USE_SERVER_TLS
#define UPLOAD_SERVER_TLS_FINGERPRINT "12:34:56:78:90:AB:CD:12:34:56:78:90:AB:CD:12:34:56:78:90:AB"
#define UPLOAD_SERVER_HOST "gps-logger.example.com"
#define UPLOAD_SERVER_PORT 443
#else
#define UPLOAD_SERVER_HOST "10.0.1.100"
#define UPLOAD_SERVER_PORT 443
#endif

#define UPLOAD_SERVER_PATH "/upload"
#define UPLOAD_SERVER_MAGIC_HEADER "X-MAGIC-AUTHORIZER"
#define UPLOAD_SERVER_MAGIC_HEADER_VALUE "some-security-through-obscurity-spam-block"
