// router.c
// Central naming authority on lights.  Also can route messages to
// lights by name.

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>

#define BUFSIZE 256

typedef struct sq_serv_light_s {
  struct sq_serv_light_s * next_light;
  char name[32];
  int light_type;
  struct sockaddr_in lightaddr;
  time_t lastalive;
} sq_serv_light_t;

static sq_serv_light_t * serv_lights = NULL;

void dump_serv_light_table(void) {
  sq_serv_light_t * curr = serv_lights;
  printf("Lights:\n");
  if(curr == NULL) {
    printf(" (none)\n");
  }
  for(;curr != NULL; curr = curr->next_light) {
    char name[33];
    strncpy(name, curr->name, 32);
    name[32] = '\0';
    printf(" %s type=%d lastalive=%ld\n",
	   name, curr->light_type, curr->lastalive);
    
  }
}

sq_serv_light_t * sq_serv_last_ptr(void) {
  sq_serv_light_t * curr = serv_lights;
  if(curr == NULL) {
    return NULL;
  }
  while(1) {
    if(curr->next_light == NULL) {
      return curr;
    }
    curr = curr->next_light;
  }
}

sq_serv_light_t * sq_serv_light_by_name(char * name) {
  sq_serv_light_t * curr = serv_lights;
  while(curr != NULL) {
    if(sqlights_eq_name(name, curr->name)) {
      return curr;
    }
    curr = curr->next_light;
  }
  return NULL;
}

static int servsock;

void sq_send_die(sq_serv_light_t * light) {
  struct sq_die msg;
  msg.type = SQ_DIE;
  sendto(servsock, (void*)&msg, sizeof(msg), 0,
	 (struct sockaddr *)&light->lightaddr, sizeof(light->lightaddr));
}

void sq_serv_send_ack(sq_serv_light_t * light) {
  struct sq_msg_ack_reg msg;
  msg.type = SQ_ACK_REG;
  strncpy(msg.name, light->name, 32);
  sendto(servsock, (void*)&msg, sizeof(msg), 0,
	 (struct sockaddr *)&light->lightaddr, sizeof(light->lightaddr));
}

void sq_add_light(char * name, int light_type,
		  struct sockaddr_in * lightaddr) {
  sq_serv_light_t * light = sq_serv_light_by_name(name);
  if(light != NULL) {
    //    sq_send_die(light);
    light->light_type = light_type;
    memcpy(&light->lightaddr, lightaddr, sizeof(struct sockaddr_in));
    light->lastalive = time(NULL);
    sq_serv_send_ack(light);
    return;
  }
  light = malloc(sizeof(sq_serv_light_t));
  light->next_light = NULL;
  strncpy(light->name, name, 32);
  light->light_type = light_type;
  memcpy(&light->lightaddr, lightaddr, sizeof(struct sockaddr_in));
  light->lastalive = time(NULL);
  sq_serv_send_ack(light);

  sq_serv_light_t * last = sq_serv_last_ptr();
  if(last == NULL) {
    serv_lights = light;
  } else {
    last->next_light = light;
  }
}

void sq_remove_light(char * name) {
  sq_serv_light_t * light = sq_serv_light_by_name(name);
  if(light != NULL) {
    //    sq_send_die(light);
    sq_serv_light_t * curr = serv_lights;
    sq_serv_light_t * lastlight = NULL;
    while(1) {
      if(curr == light) {
	if(lastlight == NULL) {
	  serv_lights = curr->next_light;
	} else {
	  lastlight->next_light = curr->next_light;
	}
	return;
      }
      lastlight = curr;
      curr = curr->next_light;
    }
  }
}

void sq_serv_remove_old() {
  sq_serv_light_t * light = serv_lights;
  char removed = 0;
 start:
  while(light != NULL) {
    if(light->lastalive + REMOVE_DELAY < time(NULL)) {
      sq_remove_light(light->name);
      light = serv_lights;
      removed = 1;
      goto start;
    }
    light = light->next_light;
  }
  if(removed) {
    dump_serv_light_table();
  }
}

void sq_serv_forward(sq_serv_light_t * light, const void * msg,
		     size_t length) {
  int ret = sendto(servsock, msg, length, 0,
		   (struct sockaddr*)&light->lightaddr, sizeof(light->lightaddr));
  if(ret < 0) {
    char buf[33];
    strncpy(buf, light->name, 32);
    buf[32] = '\0';
    printf("Lost light \"%s\"\n", buf);
    sq_remove_light(light->name);
  }
}

static struct sockaddr_in servaddr;

void sq_serv_init(void) {
  try(0 <= (servsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)),
      "Failed to create udp socket");
  int on = 1;
  setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SQ_PORT); // for "leits"

  tryp(0 <= bind(servsock, (struct sockaddr*) &servaddr, sizeof(servaddr)),
       "Failed to bind server udp socket");


}

void sq_serv_handle(void) {
  char msg[BUFSIZE];
  int recvlen;
  struct sockaddr_in clientaddr;

  sq_serv_remove_old();

  fd_set fds;
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(servsock, &fds);
  select(servsock+1, &fds, NULL, NULL, &tv);
  
  unsigned int clientlen = sizeof(clientaddr);
  recvlen = recvfrom(servsock, msg, BUFSIZE, MSG_DONTWAIT,
		     (struct sockaddr *)&clientaddr, &clientlen);
  if(recvlen < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return;
    } else {
      dieperr("sq_serv_handle recv");
    }
  }

  sq_serv_light_t * light;
  struct sq_msg_reg_light * msgreg;
  struct sq_light_onoff * msgonoff;
  struct sq_light_brightness * msgbrightness;
  struct sq_light_color * msgcolor;
  sq_msg_type type = ((struct sq_msg*)msg)->type;

  switch(type) {
  case SQ_REG_LIGHT:
    msgreg = (struct sq_msg_reg_light*)msg;
    sq_add_light(msgreg->name, msgreg->light_type, &clientaddr);
    dump_serv_light_table();
    break;

  case SQ_ACK_REG:
    // router shouldn't get this
    break;

  case SQ_CHECK_LIGHT:
    // router shouldn't get this
    break;

  case SQ_ACK_CHECK:
    msgreg = (struct sq_msg_reg_light*)msg;
    light = sq_serv_light_by_name(msgreg->name);
    if(light != NULL) {
      light->lastalive = time(NULL);
    }
    break;

  case SQ_LIGHT_ONOFF:
    msgonoff = (struct sq_light_onoff*)msg;
    light = sq_serv_light_by_name(msgonoff->name);
    if(light != NULL)
      sq_serv_forward(light, msg, sizeof(struct sq_light_onoff));
    break;
    
  case SQ_LIGHT_BRIGHTNESS:
    msgbrightness = (struct sq_light_brightness*)msg;
    light = sq_serv_light_by_name(msgbrightness->name);
    if(light != NULL)
      sq_serv_forward(light, msg, sizeof(struct sq_light_brightness));
    break;
    
  case SQ_LIGHT_RGB:
  case SQ_LIGHT_HSI:
    msgcolor = (struct sq_light_color*)msg;
    light = sq_serv_light_by_name(msgcolor->name);
    if(light != NULL)
      sq_serv_forward(light, msg, sizeof(struct sq_light_color));
    break;

  case SQ_DIE:
    // The router shouldn't even be getting this.
    break;
  }
}

int main(int argc, char **argv) {
  sq_serv_init();
  dump_serv_light_table();
  while(1) {
    sq_serv_handle();
  }
}
