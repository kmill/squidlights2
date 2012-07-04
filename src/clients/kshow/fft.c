#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <fftw3.h>
#include <math.h>
#include <jack/jack.h>
#include "protocol.h"

#define WINDOW_SIZE 2048


/*
compile with: gcc -o fft fft.c -lfftw3 -ljack -lm

gcc -o fft fft.c -I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -lfftw3 -ljack -lm
*/

/* 
 Light definitions
*/


char* long_vol_changers[] = {"red-center",
			     "green-lantern",
			     NULL};
char* short_vol_changers[] = {"cyan-back",
			      "green-bulbs",
			      NULL};
char* insens_beat_lighters[] = {"hanging-terahertz",
				"traffic-cone",
				NULL};
char* sens_beat_lighters[] = {"blue-front",
			      "eit-sight",
			      NULL};
char* supsens_beat_lighters[] = {"yellow-yield",
				 NULL};
char* sd_lighters[] = {"yellow-back",
		       NULL};
char* bass_lighters[] = {"neon",
			 "traffic-light",
			 NULL};
char* tenor_lighters[] = {"purple-mantle",
			  "blue-neons",
			  NULL};


void set_leits(char** lights, float val) {
  for(int i = 0; lights[i] != NULL; i++) {
    sqlights_client_brightness(lights[i], val);
  }
}


double * in;
fftw_complex * out;
fftw_complex * out2;
fftw_plan ff_plan;
fftw_plan ff_plan2;

jack_client_t * jclient;
jack_port_t * j_lp;

void analyze(void) {
  double sum = 0, sd = 0;
  static double avg_sd;
  int i, j;
  double volume = 0;
  static double last_volume = 0;
  float short_vol_change = 0, long_vol_change = 0;
  float sens_beat_light = 0;
  float insens_beat_light = 0;
  float supsens_beat_light = 0;
  static double short_avgvolume = 0;
  static double longer_avgvolume = 0;
  static double rate = 0;
  static int sens_beat_on_for = 0;
  static int insens_beat_on_for = 0;
  static int supsens_beat_on_for = 0;
  
  static double avg_bass_volume = 0;
  float bass_volume = 0;
  static double avg_tenor_volume = 0;
  float tenor_volume = 0;

  static float elmohue1 = 0;
  static float elmohue2 = 0;

  // Volume following
  volume = 0;
  for(i = 0; i < WINDOW_SIZE; i++) {
    volume = in[i] > volume ? in[i] : volume;
  }
/*   for(i = 0; i < WINDOW_SIZE; i++) { */
/*     volume += in[i] > 0 ? in[i] : -in[i]; */
/*   } */
/*   volume /= WINDOW_SIZE; */
  short_avgvolume = (24 * short_avgvolume + volume) / 25;
  longer_avgvolume = (49 * longer_avgvolume + volume) / 50;
  short_vol_change = 0.5+0.5*(volume - short_avgvolume)/short_avgvolume;
  long_vol_change = 0.5 + 0.5*(volume - longer_avgvolume)/longer_avgvolume;
  //  printf("lvc:%i\tsvc:%i\t", long_vol_change, short_vol_change);

  set_leits(long_vol_changers, long_vol_change);
  set_leits(short_vol_changers, short_vol_change);

  // "Beat following"
  //printf("%f ", (volume - last_volume)/longer_avgvolume);
  if((volume - last_volume)/longer_avgvolume > 0.65) {
    insens_beat_light = 1.0;
    insens_beat_on_for = 2;
    elmohue1 += rand()*240.0 + 60.0;
    elmohue1 = fmod(elmohue1, 360.0);
  } else if (insens_beat_on_for > 0) {
    insens_beat_on_for--;
    insens_beat_light = 1.0;
  } else {
    insens_beat_light = 0;
  }
  if((volume - last_volume)/longer_avgvolume > 0.45) {
    sens_beat_light = 1.0;
    sens_beat_on_for = 2;
    elmohue2 += rand()*240.0 + 60.0;
    elmohue2 = fmod(elmohue2, 360.0);
  } else if (sens_beat_on_for > 0) {
    sens_beat_on_for--;
    sens_beat_light = 1.0;
  } else {
    sens_beat_light = 0;
  }
  if((volume - last_volume)/longer_avgvolume > 0.17) {
    supsens_beat_light = 1.0;
    supsens_beat_on_for = 2;
  } else if (supsens_beat_on_for > 0) {
    supsens_beat_on_for--;
    supsens_beat_light = 1.0;
  } else {
    supsens_beat_light = 0;
  }
  //  printf("ibl:%i\tsbl:%i\t", insens_beat_light, sens_beat_light);
  set_leits(insens_beat_lighters, insens_beat_light);
  set_leits(sens_beat_lighters, sens_beat_light);
  if(!insens_beat_light && !sens_beat_light) {
    //    printf("ssb:%i\t", supsens_beat_light);
    set_leits(supsens_beat_lighters, supsens_beat_light);
  } else {
    //    printf("ssb:%i\t", 0);
    set_leits(supsens_beat_lighters, 0);
  }

  sqlights_client_hsi("elmo0", elmohue1, 1.0, 1.0); //long_vol_change);
  sqlights_client_hsi("elmo1", elmohue2, 1.0, 1.0); //short_vol_change);


  last_volume = volume;

  fftw_execute(ff_plan);

  sum = 0;
  for(i = 0; i < WINDOW_SIZE/2; i++) {
    sum += (out[i][0]*out[i][0]+out[i][1]*out[i][1]);
  }
  sum /= WINDOW_SIZE/2;
  sd = 0;
  for(i = 0; i < WINDOW_SIZE/2; i++) {
    double diff = out[i][0]*out[i][0]+out[i][1]*out[i][1] - sum;
    sd += diff * diff;
  }
  sd /= WINDOW_SIZE/2;
  avg_sd = (24 * avg_sd + sd) / 25;
  //  printf("sdl:%i\t", (int)(sd-1.8*avg_sd+111));
  set_leits(sd_lighters, (sd-1.8*avg_sd+111)/254);
  //  printf("%f\t", sd);

  // bass
  sum = 0;
  for(i = 0; i < 60; i++) {
    sum += out[i][0]*out[i][0]+out[i][1]*out[i][1];
  }
  sum /= 60;
  avg_bass_volume = (24*avg_bass_volume + sum)/25;
  bass_volume = (127+127*(sum - longer_avgvolume));
  set_leits(bass_lighters, (sum-1.2*avg_bass_volume+111)/254);

  // tenor
  sum = 0;
  for(i = 60; i < 140; i++) {
    sum += out[i][0]*out[i][0]+out[i][1]*out[i][1];
  }
  sum /= 140-60;
  avg_tenor_volume = (24*avg_tenor_volume + sum)/25;
  tenor_volume = (127+127*(sum - longer_avgvolume));
  set_leits(tenor_lighters, (sum-1.2*avg_tenor_volume+111)/254);

  // find timbre vector
  
  
/*   for(i = 0; i < WINDOW_SIZE/2;) { */
/*     sum = 0; */
/*     for(j = 0; j < 32; j++, i++) { */
/*       sum += out[i][0]*out[i][0]+out[i][1]*out[i][1]; */
/*     } */
/*     sum /= 8; */
/*     if(sum < 0.1) { */
/*       printf(" "); */
/*     } else if(sum < 0.3) { */
/*       printf("."); */
/*     } else if(sum < 0.5) { */
/*       printf("-"); */
/*     } else if(sum < 0.7) { */
/*       printf("+"); */
/*     } else { */
/*       printf("*"); */
/*     } */
/*   } */
/*   printf("%f", volume); */
//  printf("\n");

  
}

int j_receive(jack_nframes_t nframes, void * arg) {
  static int i = 0;
  int b;
  
  jack_default_audio_sample_t *lin = (jack_default_audio_sample_t*)jack_port_get_buffer(j_lp, nframes);
  
  for(b = 0; i < WINDOW_SIZE && b < nframes; b++, i++) {
    in[i] = lin[b];
  }
  if(i >= WINDOW_SIZE) {
    i = 0;
    analyze();
  }
  //  if(i < WINDOW_SIZE
  //  memcpy(in, lin, sizeof(jack_default_audio_sample_t)*nframes);
  //  fftw_execute(ff_plan);

  return 0;
}

void j_shutdown(void *arg) {
  
}

int main(int argc, char** argv) {
  char * hostname = "localhost";
  if(argc > 1) {
    hostname = argv[1];
  }
  sqlights_client_initialize(hostname);

  srand(time(NULL));

  in = (double*) fftw_malloc(sizeof(double) * WINDOW_SIZE);
  out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * WINDOW_SIZE);
  ff_plan = fftw_plan_dft_r2c_1d(WINDOW_SIZE, in, out, FFTW_DESTROY_INPUT);

  printf("Connecting to jack...\n");
  if(!(jclient = jack_client_open("leitshow", JackNoStartServer, NULL))) {
    fprintf(stderr, "Cannot connect to jack.\n");
    return 1;
  }
  printf("Connected.\n");
  
  jack_set_process_callback(jclient, j_receive, 0);
  jack_on_shutdown(jclient, j_shutdown, 0);

  printf("set jack callbacks\n");

  j_lp = jack_port_register(jclient, "left_input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  
  if(jack_activate(jclient)) {
    fprintf(stderr, "Cannot activate jack client.\n");
    return 1;
  }
  
  printf("activated jack client\nHere we go!!!\n");

  //  scanf("Hit enter to quit\n");
  for(;;)
    sleep(1);
  
  jack_client_close(jclient);
  
  return 0;
}
