#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>

#include <arpa/inet.h>

#define MAXDATASIZE 512

typedef struct hot_potato{
  int hops;
  int trace[512];
  int count;
  //int num_players;
  int game_over_flag;
}potato_t;

void error_message(const char *str){
  perror(str);
  exit(EXIT_FAILURE);
}


void send_full(int end_fd, potato_t *potato_ptr){
  int check_send = send(end_fd, potato_ptr, sizeof(*potato_ptr), 0);
  if(check_send < sizeof(*potato_ptr)){
    printf("to send full\n");
    check_send += send(end_fd, (potato_ptr+check_send), (sizeof(*potato_ptr)-check_send), 0);
  }
}

/*
void send_full(int end_fd, void *message, int m_size){
  int check_send = send(end_fd, message, m_size, 0);
  if(check_send < sizeof(*message)){
    check_send += send(end_fd, (message+check_send), (m_size-check_send), 0);
  }
}

*/
