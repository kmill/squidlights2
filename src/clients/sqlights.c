/* light control by the command line */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void print_usage(char* prgname) {
    printf("usage: %s\n"
	   "\tlist\n"
	   "\ton (lightname)\n"
	   "\toff (lightname)\n"
	   //	   "\tset (lightname) (brightness)\n"
	   //	   "\trgb (lightname) (r) (g) (b)\n"
	   //	   "\thsi (lightname) (h) (s) (i)\n\n"
	   //	   "use . for lightname to send the signal to all lights\n",
	   ,
	   prgname);
}

float read_arg_float(int argc, char** argv, int i) {
  if(i < 0) return 0;
  if(i >= argc) return 0;
  return (float)atof(argv[i]);
}

// assumes enough arguments.
void handle_command(int argc, char** argv) {
  if(strcmp(argv[2], "on")==0) {
    sqlights_client_seton(argv[3], 1);
  } else if(strcmp(argv[2], "off")==0) {
    sqlights_client_seton(argv[3], 0);
  }
  /* else if(strcmp(argv[1], "set")==0) { */
  /*   float b = read_arg_float(argc, argv, 3); */
  /*   printf("turning %s to %f\n", squidlights_client_lightname(lightid), b); */
  /*   squidlights_client_light_set(clientid,lightid, b); */
  else if(strcmp(argv[2], "rgb")==0) {
    float r = read_arg_float(argc, argv, 4);
    float g = read_arg_float(argc, argv, 5);
    float b = read_arg_float(argc, argv, 6);
    printf("turning %s to rgb=(%f,%f,%f)\n", argv[3], r,g,b);
    sqlights_client_rgb(argv[3], r, g, b);
  }
  /* } else if(strcmp(argv[1], "hsi")==0) { */
  /*   float h = read_arg_float(argc, argv, 3); */
  /*   float s = read_arg_float(argc, argv, 4); */
  /*   float i = read_arg_float(argc, argv, 5); */
  /*   printf("turning %s to hsi=(%f,%f,%f)\n", squidlights_client_lightname(lightid), h,s,i); */
  /*   squidlights_client_light_hsi(clientid,lightid, h,s,i); */
  /* } */
}

int main(int argc, char** argv) {
  char * hostname = "localhost";
  if(argc > 1) {
    hostname = argv[1];
  }
  sqlights_client_initialize(hostname);

  if(argc == 1) {
    print_usage(argv[0]);
  } else {
    if(argc < 3) {
      print_usage(argv[0]);
    } else {
      handle_command(argc, argv);
    }
  }

}
