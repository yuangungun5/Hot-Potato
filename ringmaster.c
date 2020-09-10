#include "potato.h"
#include <time.h>

#define BACKLOG 512

void init_print(int num_players, int num_hops){
  printf("Potato Ringmaster \nPlayers = %d \nHops = %d \n", num_players, num_hops);
}

void build_ring(int next_player_fd, char *port, char *ip){
  send(next_player_fd, port, 32, 0);
  send(next_player_fd, ip, 32, 0);
}

int main(int argc, char* argv[]){

  if (argc != 4){
    error_message("Invalid format! Please enter: ringmaster <port_num> <num_players> <num_hops>\n");
  }

  //int port_num = atoi(argv[1]);
  int num_players = atoi(argv[2]);
  int num_hops = atoi(argv[3]);
  int *player_sock_fd = malloc(sizeof(*player_sock_fd) * num_players); // record the descriptor for each player
  int *player_listen = malloc(sizeof(*player_listen) * num_players);
  //int *player_ip = malloc(sizeof(*player_ip) * num_players);
  struct sockaddr_in *player_addr_s = malloc(sizeof(*player_addr_s) * num_players);

  /*
  char **player_listen = malloc(sizeof(*player_listen) * num_players); // port
  char **player_ip = malloc(sizeof(*player_ip) * num_players); // hostname

  for (int i=0;i<num_players;i++){
    player_listen[i] = malloc(sizeof(**player_listen) * 512);
    player_ip[i] = malloc(sizeof(**player_ip) * 512);
  }
  */

  assert(num_players > 1);
  assert(num_hops >= 0);
  assert(num_hops <= 512);
  // print starting messages
  init_print(num_players, num_hops);

  // get address for host
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags    = AI_PASSIVE;
  int status = getaddrinfo(NULL, argv[1], &host_info, &host_info_list);
  if (status != 0) {
    error_message("Error: cannot get address info for host\n");
  }

  // create the server socket
  int socket_fd = socket(host_info_list->ai_family,
			 host_info_list->ai_socktype,
			 host_info_list->ai_protocol);
  if (socket_fd == -1) {
    error_message("Error: cannot create socket\n");
  }

  
  int yes = 1;
  status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    error_message("Error: cannot bind socket\n");
  }

  status = listen(socket_fd, BACKLOG);
  if (status == -1) {
    error_message("Error: cannot listen on socket\n");
  }

  // set up socket connections between ringmaster and players
  for (int i=0;i<num_players;i++){
    struct sockaddr_in player_addr;
    socklen_t addrlen = sizeof(player_addr);
    int player_fd = accept(socket_fd, (struct sockaddr *) &player_addr, &addrlen);
    if (player_fd == -1){
      error_message("Error: cannot accept connection on socket\n");
    }
    player_sock_fd[i] = player_fd;
    player_addr_s[i] = player_addr;

    // send player id and number of players

    char id [32];
    memset(id, '\0', 32);
    sprintf(id, "%d", i);
    send(player_fd, id, sizeof(id), 0);
    char num[32];
    memset(num, '\0', 32);
    sprintf(num, "%d", num_players);
    send(player_fd, num, sizeof(num), 0);
    
    // receive message of port for listening from player
    char message[32];
    memset(message, '\0', 32);
    //int message;
    recv(player_fd, message, sizeof(message), 0);
    player_listen[i] = atoi(message);
    printf("Player %d is ready to play\n", i);

  }

  // check
  /*
  for (int i=0;i<num_players;i++){
    printf("player_listen[%d] = %d\n", i, player_listen[i]);
    printf("player_ip[%d] = %s\n", i, inet_ntoa((&player_addr_s[i])->sin_addr));
  }
  */
  
  // build a ring of processes
  // send ip and listening port to a neighbour
  for(int i=0;i<num_players;i++){
    int n = (i + 1) % num_players;
    char lis_port[32];
    memset(lis_port, '\0', 32);
    sprintf(lis_port, "%d", player_listen[i]);
    char player_ip[32];
    memset(player_ip, '\0', 32);
    sprintf(player_ip, "%s", inet_ntoa((&player_addr_s[i])->sin_addr));
    build_ring(player_sock_fd[n], lis_port, player_ip);
  }

  // confirm connections
  int c = 0;
  while(1){
    int flag;
    char flag_message[8];
    memset(flag_message, '\0', 8);
    recv(player_sock_fd[c], flag_message, sizeof(flag_message), 0);
    flag = atoi(flag_message);
    if (flag == 1){
      c++;
    }
    if (c == num_players){
      break;
    }
  }
  
  // game starts
  potato_t potato;
  //printf("size of potato: %ld\n", sizeof(potato));
  potato.hops = num_hops;
  //potato.trace = malloc(sizeof(int) * num_hops);
  //memset(potato.trace, 0, num_hops);
  memset(potato.trace, 0, 512 * sizeof(potato.trace[0]));
  potato.count = 0;
  potato.game_over_flag = 0;

  if(num_hops == 0){        
    // game over
    potato.game_over_flag = 1;
    // close
    for(int i=0;i<num_players;i++){
      //send(player_sock_fd[i], &potato, sizeof(potato), 0);
      send_full(player_sock_fd[i], &potato);
      close(player_sock_fd[i]);
    }
    close(socket_fd);
    freeaddrinfo(host_info_list);

    // free
    free(player_sock_fd);
    free(player_listen);
    free(player_addr_s);
    //free(player_ip);
    // exit
    return 1;
  }

  srand ((unsigned int)(time(NULL)) + num_hops);
  int start = rand() % num_players;
  printf("Ready to start the game, sending potato to player %d\n", start);
  
  //send(player_sock_fd[start], &potato, sizeof(potato), 0);
  send_full(player_sock_fd[start], &potato);
  
  // receive potato from the last player
  fd_set readfds;
  int max_fd = 0;

  FD_ZERO(&readfds);
  for (int i=0;i<num_players;i++){
    FD_SET(player_sock_fd[i], &readfds);
    if (player_sock_fd[i] > max_fd){
      max_fd = player_sock_fd[i];
    }
  }
  select(max_fd+1, &readfds, NULL, NULL, NULL);
  
  // end
  potato_t updated_potato;
  memset(updated_potato.trace, 0, 512 * sizeof(potato.trace[0]));
  size_t check_recv;
  
  for (int j=0;j<num_players;j++){
    if (FD_ISSET(player_sock_fd[j], &readfds)){
      check_recv = recv(player_sock_fd[j], &updated_potato, sizeof(updated_potato), MSG_WAITALL);
      //printf("receive message size: %ld\n", check_recv);
      //printf("game_over flag: %d\n", updated_potato.game_over_flag);
      if(check_recv!=sizeof(updated_potato)) {
	continue;
      }
      updated_potato.game_over_flag = 1;
   
      break;
    }
  }

  
  // tell every player game over
  for (int i=0;i<num_players;i++){
    /*
    int check_send = send(player_sock_fd[i], &potato, sizeof(potato), 0);
    while(check_send != sizeof(potato)){
      printf("fail to send!\n");
      send(player_sock_fd[i], &potato, sizeof(potato), 0);
    }
    */

    send_full(player_sock_fd[i], &updated_potato);
    //close(player_sock_fd[i]);
  }
  for (int i=0;i<num_players;i++){
    close(player_sock_fd[i]);
  }

  printf("Trace of potato:\n");
  for(int i=0;i<num_hops;i++){
    printf("%d", updated_potato.trace[i]);
    if (i < updated_potato.hops - 1){
      printf(", ");
    }
    else{
      printf("\n");
    }
  }
  
  free(player_sock_fd);
  free(player_listen);
  free(player_addr_s);
  close(socket_fd);
  freeaddrinfo(host_info_list);

  //free(potato.trace);
    
  // self check
  int player_receive_potato [num_players];
  memset(player_receive_potato, 0, num_players * sizeof(int));
  for (int i=0;i<num_players;i++){
    //printf("Trace of player %d: \n", i);
    for(int j=0;j<num_hops;j++){
      if(updated_potato.trace[j]==i){
	  player_receive_potato[i] += 1;
      }
    }
    //printf("player %d receives potato %d times.\n", i, player_receive_potato[i]);
  }
  
  
  
  return EXIT_SUCCESS;
}
