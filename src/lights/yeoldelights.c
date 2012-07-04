/* yeoldelights.c
   this program controls the "old" light show (that is, the serial-controlled lights */

/* yeoldelights.conf has the format "a b c d" where a=0,1 for whether
   can handle different brightnesses, b&c are the address on the
   serial controller, and d is a unique whitespace-less string name
   for the light */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static int leitshow_handle;

/* Returns the handle for the serial port */
int connect_to_leitshow(char* device) {
  struct termios options;
  leitshow_handle = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
  if(leitshow_handle < 0) {
    fprintf(stderr, "Cannot connect to %s.", device);
    return -1;
  }
  tcgetattr(leitshow_handle, &options);
  cfsetispeed(&options, B19200);
  cfsetspeed(&options, B19200);
  options.c_cflag |= CLOCAL;
  //  options.c_cflag &= ~(CRTS_IFLOW | CCTS_OFLOW);
  options.c_lflag &= ~ICANON;
  tcsetattr(leitshow_handle, TCSAFLUSH, &options);
  tcflush(leitshow_handle, TCIOFLUSH);
  return 0;
}

void send_leitshow_packet(char lightaddr, int brightness, unsigned char period) {
  unsigned char buffer[4] = {0xFF, 0x00, 0x00, 0x00};
  if(brightness > 254) {
    brightness = 254;
  }
  if(period == 0xFF) period = 0xFE;
  buffer[1] = lightaddr;
  buffer[2] = (unsigned char)brightness;
  buffer[3] = period;
  write(leitshow_handle, buffer, 4);
}

/* brightness-able light controllers */
void ba_yelight_brightness_handler(light_t* light, float brightness) {
  if(brightness < 0) brightness = 0;
  if(brightness > 1) brightness = 1;
  send_leitshow_packet((char)((int)(light->extra_data)), (int)(254*brightness), 0);
}
void ba_yelight_onoff_handler(light_t* light, char seton) {
  ba_yelight_brightness_handler(light, seton?1.0:0.0);
}

/* brightness-unable light controllers */
/* "of" means "on/off" */
void of_yelight_onoff_handler(light_t* light, char seton) {
  send_leitshow_packet((char)((int)(light->extra_data)), seton?254:0, 0);
}

int load_lights(char * filename) {
  FILE * fp = fopen(filename, "r");
  if(fp == 0) {
    printf("couldn't open file %s\n", filename);
    return -1;
  }
  char name[256];
  int addr1, addr2;
  int hasbrightness;
  int ret;
  while((ret=fscanf(fp, "%d %d %d %s", &hasbrightness, &addr1, &addr2, name)) != EOF) {
    if(ret < 3) {
      printf("parsing error for %s\n", filename);
      fclose(fp);
      return -1;
    }
    printf("adding light \"%s\"\n", name);
    /* create the light */
    sq_light_type capab = hasbrightness?SQ_FADEABLE:SQ_ONOFF;
    light_t * light = sqlights_add_light(name, capab);

    /* attach its address on the serial controller */
    light->extra_data = (void*)((int)(32*addr1 + addr2));
    /* attach the appropriate handlers */
    if(hasbrightness) {
      light->onoff_handler = &ba_yelight_onoff_handler;
      light->brightness_handler = &ba_yelight_brightness_handler;
    } else {
      light->onoff_handler = &of_yelight_onoff_handler;
    }
  }
  printf("finished adding lights.\n");
  return 0;
}

int main(int argc, char** argv) {
  char * hostname = "localhost";
  if(argc > 1) {
    hostname = argv[1];
  }

  if(connect_to_leitshow("/dev/ttyS0")) {
    printf("serial error\n");
    exit(1);
  }
  if(sqlights_light_initialize(hostname)) {
    printf("couldn't initialize squidlights\n");
    exit(1);
  }
  char * filename = "yeoldelights.conf";
  if(argc > 1) {
    filename = argv[1];
  }
  if(load_lights(filename)) {
    printf("couldn't load lights\n");
    exit(1);
  }
  sqlights_lights_run();
}
