#ifndef _squidlights_protocol_h
#define _squidlights_protocol_h

#define SQ_PORT 13172
#define SQ_OSC_PORT 13173
#define ACK_DELAY 1
#define REACK_DELAY 5
#define REMOVE_DELAY 10

enum sq_light_type_e {
  SQ_ONOFF = 1,
  SQ_FADEABLE = 2,
  SQ_COLORED = 3
};
typedef enum sq_light_type_e sq_light_type;


struct light_s {
  char name[32];
  int light_type;
  char acked; // whether the router's acknowledged this light
  void * extra_data;
  void (*onoff_handler)(struct light_s * light, char seton);
  void (*brightness_handler)(struct light_s * light, float brightness);
  void (*rgb_handler)(struct light_s * light, float r, float g, float b);
  void (*hsi_handler)(struct light_s * light, float h, float s, float i);
};
typedef struct light_s light_t;

enum sq_msg_e {
  SQ_REG_LIGHT = 1,
  SQ_ACK_REG,
  SQ_CHECK_LIGHT,
  SQ_ACK_CHECK,
  SQ_LIGHT_ONOFF,
  SQ_LIGHT_BRIGHTNESS,
  SQ_LIGHT_RGB,
  SQ_LIGHT_HSI,
  SQ_DIE
};

typedef enum sq_msg_e sq_msg_type;

struct sq_msg {
  sq_msg_type type;
  void * data;
};

// light sends this to register a light.  If type=SQ_ACK_CHECK, then
// is in response to sq_check_light.
struct sq_msg_reg_light {
  sq_msg_type type;
  sq_light_type light_type;
  char name[32];
};

// server sends this to acknowledge reg
struct sq_msg_ack_reg {
  sq_msg_type type;
  char name[32];
};

// server sends this to check a light still exists
struct sq_check_light {
  sq_msg_type type;
  char name[32];
};

// server sends this to set light on/off
struct sq_light_onoff {
  sq_msg_type type;
  char name[32];
  char seton;
};

// server sends this to set brightness of light
struct sq_light_brightness {
  sq_msg_type type;
  char name[32];
  float brightness;
};

// server sends this to set rgb of light
struct sq_light_color {
  sq_msg_type type;
  char name[32];
  union {
    struct {
      float r, g, b;
    } rgb;
    struct {
      float h, s, i;
    } hsi;
  } color;
};

// server sends this to kill all lights.  lights send this to remove
// themselves from the server.
struct sq_die {
  sq_msg_type type;
};

int sqlights_eq_name(char * n1, char * n2);

/*** client functions ***/

int sqlights_client_initialize(char * routeraddr);

void sqlights_client_seton(char * name, char seton);
void sqlights_client_brightness(char * name, float brightness);
void sqlights_client_rgb(char * name, float r, float g, float b);
void sqlights_client_hsi(char * name, float h, float s, float i);

/*** light functions ***/

// initializes the light system for this process
int sqlights_light_initialize(char * routeraddr);

// adds a light, returns the light structure.
light_t * sqlights_add_light(char * name, sq_light_type capabilities);

// gets a light by name
light_t * sqlights_get_light(char * name);

// removes a light by name
void sqlights_del_light(char * name);

// once set up, just runs the lights
void sqlights_lights_run(void);
// or, do one iteration of running the lights.  If wait is true, then
// do a blocking call.  Returns 0 if handled something, -1 otherwise.
int sqlights_lights_handle(char wait);

/** helpful functions **/

void dieperr(const char *msg);
void die(const char *msg);
void tryp(int ret, const char * msg);
void try(int ret, const char * msg);

#endif
