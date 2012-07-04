// lights.c
// implementation of protocol.h

#include "protocol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#define BUFSIZE 256
#define notok(x) ((x) < 0)

void dieperr(const char *msg) {
  perror(msg);
  exit(1);
}
void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}
void tryp(int ret, const char * msg) {
  if(!ret)
    dieperr(msg);
}
void try(int ret, const char * msg) {
  if(!ret)
    die(msg);
}

// dst should be a buffer at least 33 characters long.
void sq_get_name(char * dst, light_t * light) {
  strncpy(dst, light->name, 32);
  dst[32] = '\0';
}

void default_onoff_handler(light_t * light, char seton) {
  char name_buf[33];
  sq_get_name(name_buf, light);
  printf("default onoff handler for %s\n", name_buf);
}
void default_brightness_handler(light_t * light, float brightness) {
  if(brightness > 0.5) {
    light->onoff_handler(light, 1);
  } else {
    light->onoff_handler(light, 0);
  }
}
void default_rgb_handler(light_t * light, float r, float g, float b) {
  float brightness = (r + g + b)/3;
  light->brightness_handler(light, brightness);
}
static inline float deg_to_rad(float d) {
  return d*M_PI/180.0;
}
void default_hsi_handler(light_t * light, float h, float s, float i) {
  float r, g, b;
  h = fmod(h, 360.0);
  if(h < 120) {
    r = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    g = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    b = (1-s)/3;
  } else if(h < 240) {
    h = h - 120;
    g = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    b = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    r = (1-s)/3;
  } else {
    h = h - 240;
    b = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    r = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    g = (1-s)/3;
  }
  light->rgb_handler(light, r, g, b);
}

struct light_list_s {
  struct light_list_s * next_light;
  light_t light;
};

int sqlights_eq_name(char * n1, char * n2) {
  for(int i = 0; i < 32; i++) {
    if(n1[i] != n2[i]) {
      return 0;
    }
    if(n1[i] == '\0') {
      return 1;
    }
  }
  return 1;
}

char * sqlights_name_cpy(char * dest, char * src) {
  return strncpy(dest, src, 32);
}

static struct light_list_s * lights = NULL;
static struct sockaddr_in servaddr;
static int udpsock;
static time_t ack_next;
static time_t reack_next;

// initializes the light system for this process
int sqlights_light_initialize(char * routeraddr) {
  struct hostent *host;
  
  tryp(0 <= (udpsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)),
       "Failed to create udp socket");
  //fctl(udpsock, F_SETFL, O_NONBLOCK);
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SQ_PORT);
  try(NULL != (host = gethostbyname(routeraddr)),
      "Invalid host name");
  bcopy(host->h_addr, &servaddr.sin_addr, host->h_length);
  ack_next = time(NULL) + ACK_DELAY;
  reack_next = time(NULL) + REACK_DELAY;
  return 0;
}

int sqlights_light_sendto(int udpsock, const void * msg, size_t length) {
  tryp(0 <= sendto(udpsock, msg, length, 0,
		   (struct sockaddr *) &servaddr, sizeof(servaddr)),
       "sqlights_light_send_packet()");
  return 0;
}

void sqlights_light_send_reg(light_t * light) {
  struct sq_msg_reg_light msg;
  msg.type = SQ_REG_LIGHT;
  msg.light_type = light->light_type;
  sqlights_name_cpy(msg.name, light->name);
  sqlights_light_sendto(udpsock, (void*)&msg, sizeof(msg));
}
void sqlights_light_send_ack(light_t * light) {
  struct sq_msg_reg_light msg;
  msg.type = SQ_ACK_CHECK;
  msg.light_type = light->light_type;
  sqlights_name_cpy(msg.name, light->name);
  sqlights_light_sendto(udpsock, (void*)&msg, sizeof(msg));
}

struct light_list_s * sq_light_last_ptr(void) {
  struct light_list_s * curr = lights;
  while(curr != NULL) {
    if(curr->next_light == NULL) {
      return curr;
    }
    curr = curr->next_light;
  }
  return NULL;
}

// adds a light, returns the light id.
light_t * sqlights_add_light(char * name, sq_light_type capabilities) {
  struct light_list_s * new_light_list;
  struct light_list_s * last_light_ptr = sq_light_last_ptr();
  light_t * light;
  
  new_light_list = malloc(sizeof(struct light_list_s));
  new_light_list->next_light = NULL;
  light = &new_light_list->light;

  sqlights_name_cpy(light->name, name);
  light->extra_data = NULL;
  light->acked = 0;
  light->onoff_handler = &default_onoff_handler;
  light->brightness_handler = &default_brightness_handler;
  light->rgb_handler = &default_rgb_handler;
  light->hsi_handler = &default_hsi_handler;

  // insert it into the list "lights"
  if(last_light_ptr == NULL) {
    lights = last_light_ptr = new_light_list;
  } else {
    last_light_ptr->next_light = new_light_list;
    last_light_ptr = new_light_list;
  }

  sqlights_light_send_reg(light);
  return light;
}

// gets a light by name
light_t * sqlights_get_light(char * name) {
  struct light_list_s * currlight = lights;
  while(currlight != NULL) {
    if(sqlights_eq_name(name, currlight->light.name)) {
      return &currlight->light;
    }
    currlight = currlight->next_light;
  }
  return NULL;
}

// removes a light by name
void sqlights_del_light(char * name) {
  struct light_list_s * currlight = lights;
  struct light_list_s * lastlight = NULL;
  while(currlight != NULL) {
    if(sqlights_eq_name(name, currlight->light.name)) {
      if(lastlight == NULL) {
	lights = currlight->next_light;
      } else {
	lastlight->next_light = currlight->next_light;
      }
      return;
    }
    lastlight = currlight;
    currlight = currlight->next_light;
  }
}

void sqlights_clear_acks() {
  struct light_list_s * currlight = lights;
  while(currlight != NULL) {
    currlight->light.acked = 0;
    currlight = currlight->next_light;
  }
}

void sqlights_reg_unacked_lights() {
  struct light_list_s * currlight = lights;
  while(currlight != NULL) {
    if(!currlight->light.acked) {
      sqlights_light_send_reg(&currlight->light);
    }
    currlight = currlight->next_light;
  }
}

// once set up, just runs the lights
void sqlights_lights_run(void) {
  printf("running...\n");
  while(1) {
    sqlights_lights_handle(1);
  }
}

// or, do one iteration of running the lights.  If wait is true, then
// do a blocking call (with a timeout of 1 sec)
int sqlights_lights_handle(char wait) {
  char msg[BUFSIZE];
  int ret;
  time_t currtime = time(NULL);
  
  if(currtime >= reack_next) {
    reack_next = time(NULL) + REACK_DELAY;
    sqlights_clear_acks();
  }
  if(currtime >= ack_next) {
    ack_next = time(NULL) + ACK_DELAY;
    sqlights_reg_unacked_lights();
  }

  if(wait) {
    fd_set fds;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(udpsock, &fds);
    select(udpsock+1, &fds, NULL, NULL, &tv);
  }

  ret = recv(udpsock, msg, BUFSIZE, MSG_DONTWAIT);
  if(ret < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return -1;
    } else {
      dieperr("sqlights_light_handle recv");
    }
  }

  light_t * light;
  struct sq_light_onoff * msgonoff;
  struct sq_light_brightness * msgbrightness;
  struct sq_light_color * msgcolor;
  sq_msg_type type = ((struct sq_msg*)msg)->type;

  switch(type) {
    
  case SQ_REG_LIGHT:
    // don't care.
    break;
    
  case SQ_ACK_REG:
    light = sqlights_get_light(((struct sq_msg_ack_reg*)msg)->name);
    light->acked = 1;
    printf("reg acked\n");
    break;
    
  case SQ_CHECK_LIGHT:
    light = sqlights_get_light(((struct sq_check_light*)msg)->name);
    sqlights_light_send_ack(light);
    break;
    
  case SQ_ACK_CHECK:
    // don't care
    break;
    
  case SQ_LIGHT_ONOFF:
    msgonoff = (struct sq_light_onoff*)msg;
    light = sqlights_get_light(msgonoff->name);
    light->onoff_handler(light, msgonoff->seton);
    break;
    
  case SQ_LIGHT_BRIGHTNESS:
    msgbrightness = (struct sq_light_brightness*)msg;
    light = sqlights_get_light(msgbrightness->name);
    light->brightness_handler(light, msgbrightness->brightness);
    break;
    
  case SQ_LIGHT_RGB:
    msgcolor = (struct sq_light_color*)msg;
    light = sqlights_get_light(msgcolor->name);
    light->rgb_handler(light,
		       msgcolor->color.rgb.r,
		       msgcolor->color.rgb.g,
		       msgcolor->color.rgb.b);
    break;
    
  case SQ_LIGHT_HSI:
    msgcolor = (struct sq_light_color*)msg;
    light = sqlights_get_light(msgcolor->name);
    light->hsi_handler(light,
		       msgcolor->color.hsi.h,
		       msgcolor->color.hsi.s,
		       msgcolor->color.hsi.i);
    break;
    
  case SQ_DIE:
    printf("server-induced death.  Bye!\n");
    exit(0);
    break;
    
  default:
    fprintf(stderr, "Unknown message type %d\n", type);
    break;
  }

  return 0;
}

static struct sockaddr_in clservaddr;
static int cludpsock;

// initializes the light system for this process
int sqlights_client_initialize(char * routeraddr) {
  struct hostent *host;
  
  tryp(0 <= (cludpsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)),
       "Failed to create udp socket");
  //fctl(udpsock, F_SETFL, O_NONBLOCK);
  memset(&clservaddr, 0, sizeof(clservaddr));
  clservaddr.sin_family = AF_INET;
  clservaddr.sin_port = htons(SQ_PORT);
  try(NULL != (host = gethostbyname(routeraddr)),
      "Invalid host name");
  bcopy(host->h_addr, &clservaddr.sin_addr, host->h_length);
  return 0;
}

void sq_client_sendto(const void * msg, size_t length) {
  tryp(0 <= sendto(cludpsock, msg, length, 0,
		   (struct sockaddr *) &clservaddr, sizeof(clservaddr)),
       "sq_client_sendto()");
}

void sqlights_client_seton(char * name, char seton) {
  struct sq_light_onoff msg;
  msg.type = SQ_LIGHT_ONOFF;
  strncpy(msg.name, name, 32);
  msg.seton = seton;
  sq_client_sendto((void*)&msg, sizeof(msg));
}

void sqlights_client_brightness(char * name, float brightness) {
  struct sq_light_brightness msg;
  msg.type = SQ_LIGHT_BRIGHTNESS;
  strncpy(msg.name, name, 32);
  msg.brightness = brightness;
  sq_client_sendto((void*)&msg, sizeof(msg));
}

void sqlights_client_rgb(char * name, float r, float g, float b) {
  struct sq_light_color msg;
  msg.type = SQ_LIGHT_RGB;
  strncpy(msg.name, name, 32);
  msg.color.rgb.r = r;
  msg.color.rgb.g = g;
  msg.color.rgb.b = b;
  sq_client_sendto((void*)&msg, sizeof(msg));
}

void sqlights_client_hsi(char * name, float h, float s, float i) {
  struct sq_light_color msg;
  msg.type = SQ_LIGHT_HSI;
  strncpy(msg.name, name, 32);
  msg.color.hsi.h = h;
  msg.color.hsi.s = s;
  msg.color.hsi.i = i;
  sq_client_sendto((void*)&msg, sizeof(msg));
}
