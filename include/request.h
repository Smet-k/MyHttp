#define MAX_HEADERS 100
#define PATH_SIZE 512
#define URL_SIZE 256
#define VERSION_SIZE 16
#define METHOD_SIZE 8

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_UNKNOWN
} http_method_t;

typedef struct {
    http_method_t method;
    char path[PATH_SIZE];
    char version[VERSION_SIZE];
} http_request_line_t;

typedef struct {
    char name[64];
    char value[256];
} http_header_t;

typedef struct {
    http_request_line_t request_line;
    http_header_t headers[MAX_HEADERS];
    int header_count;
    char* body;
    int content_length;
} http_request_t;

http_request_t parse_request(char buf[]);