/* test light.  Creates two "lights".  The first is on/off, the second
   is brightness controlled. */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>

void print_states(void);

static int light0_state = 0;

void light0_onoff_handler(light_t * light, char seton) {
  light0_state = seton;
  print_states();
}

static int light1_brightness = 0;
void light1_brightness_handler(light_t * light, float brightness) {
  light1_brightness = (int)(254*brightness);
  print_states();
}
void light1_onoff_handler(light_t * light, char seton) {
  light1_brightness_handler(light, seton?1.0:0.0);
}

void print_states(void) {
  if(light0_state) {
    printf("0:*\t1:");
  } else {
    printf("0: \t1:");
  }
  for(int i = 0; i < light1_brightness; i += 20) {
    printf("+");
  }
  printf("\n");
}

int main(int argc, char** argv) {
  char * hostname = "localhost";
  if(argc > 1) {
    hostname = argv[1];
  }

  sqlights_light_initialize(hostname);
  light_t * light0 = sqlights_add_light("testlight_light0", SQ_ONOFF);
  light0->onoff_handler = &light0_onoff_handler;

  light_t * light1 = sqlights_add_light("testlight_light1", SQ_FADEABLE);
  light1->onoff_handler = &light1_onoff_handler;
  light1->brightness_handler = &light1_brightness_handler;

  print_states();
  sqlights_lights_run();
}
