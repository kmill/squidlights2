/** Connects OSC to squidlights using liblo **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lo/lo.h>
#include "protocol.h"

int onoff_handler(const char *path, const char *types,
		  lo_arg **argv, int argc,
		  void *msg, void *user_data) {
  char * name = &argv[0]->s;
  char seton = argv[1]->i ? 1 : 0;
  sqlights_client_seton(name, seton);
  return 0;
}
int brightness_handler(const char *path, const char *types,
		       lo_arg **argv, int argc,
		       void *msg, void *user_data) {
  char * name = &argv[0]->s;
  float brightness = argv[1]->f;
  sqlights_client_brightness(name, brightness);
  return 0;
}
int rgb_handler(const char *path, const char *types,
		lo_arg **argv, int argc,
		void *msg, void *user_data) {
  char * name = &argv[0]->s;
  sqlights_client_rgb(name, argv[1]->f, argv[2]->f, argv[3]->f);
  return 0;
}
int hsi_handler(const char *path, const char *types,
		lo_arg **argv, int argc,
		void *msg, void *user_data) {
  char * name = &argv[0]->s;
  sqlights_client_hsi(name, argv[1]->f, argv[2]->f, argv[3]->f);
  return 0;
}

void error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int main(int argc, char** argv) {
  char buffer[256];
  char * hostname = "localhost";
  if(argc > 1) {
    hostname = argv[1];
  }
  printf("Starting osc->squidlights bridge\n");
  sqlights_client_initialize(hostname);

  sprintf(buffer, "%d", SQ_OSC_PORT);

  lo_server_thread st = lo_server_thread_new(buffer, error);
  lo_server_thread_add_method(st, "/set", "si", onoff_handler, NULL);
  lo_server_thread_add_method(st, "/fade", "sf", brightness_handler, NULL);
  lo_server_thread_add_method(st, "/bright", "sf", brightness_handler, NULL);
  lo_server_thread_add_method(st, "/rgb", "sfff", rgb_handler, NULL);
  lo_server_thread_add_method(st, "/hsi", "sfff", hsi_handler, NULL);
  lo_server_thread_start(st);
  printf("Started.\n");
  for(;;) {
    usleep(1000);
  }
}
