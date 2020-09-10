#include "potato.h"
#include <time.h>

int build_listen(int *sock_fd){
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;
  
  int status = getaddrinfo(NULL, "", &host_info, &host_info_list);
  if (status != 0){
    error_message("Error: cannot get address info for host\n");
  }

  // specify 0 in sin_port to let OS assign a port
  struct sockaddr_in *addr = (struct sockaddr_in *)(host_info_list->ai_addr);
  addr->sin_port = 0;
  
  *sock_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
		  host_info_list->ai_protocol);
  if (*sock_fd == -1) {
    error_message("Error: cannot create socket\n");
  }

  int yes = 1;
  status = setsockopt(*sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(*sock_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    error_message("Error: cannot bind socket\n");
  }

  status = listen(*sock_fd, 100);
  if (status == -1) {
    error_message("Error: cannot listen on socket\n");
  }
  
  struct sockaddr_in sock;
  socklen_t len = sizeof(sock);
  getsockname(*sock_fd, (struct sockaddr *) &sock, &len);
  int listen_port = ntohs(sock.sin_port);
  //printf("listen port: %d\n", listen_port);

  freeaddrinfo(host_info_list);
  
  return listen_port;
}


int connect_neighbour(char *port, char *hostname){
  //printf("neighbour port: %s\n", port);
  //printf("neighbour hostname: %s\n", hostname);
  
  struct addrinfo host_info;
  struct addrinfo *host_info_list;

  memset(&host_info, 0, sizeof(struct addrinfo));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    error_message("Error: cannot get address info for host\n");
  }

  int neighbour_fd = socket(host_info_list->ai_family,
		     host_info_list->ai_socktype,
		     host_info_list->ai_protocol);

  if (neighbour_fd == -1) {
    error_message("Error: cannot create socket\n");
  }
  
  // here socket_fd is the descriptor of who?
  status = connect(neighbour_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    error_message("Error: cannot connect to socket\n"); 
  }

  freeaddrinfo(host_info_list);
  
  return neighbour_fd;
  
}

int main(int argc, char* argv[]){

  if (argc != 3){
    error_message("Invalid format! Please enter: player <machine_name> <port_num>\n");
  }

  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = argv[1];
  const char *port     = argv[2];

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0){
    error_message("Error: cannot get address info for host\n");
  }

  // socket_fd here is for communication with ringmaster?
  socket_fd = socket(host_info_list->ai_family,
		     host_info_list->ai_socktype,
		     host_info_list->ai_protocol);

  if (socket_fd == -1) {
    error_message("Error: cannot create socket\n");
  }
  
  status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    error_message("Error: cannot connect to socket\n"); 
  }

  //printf("connected with ringmaster\n");

  // after connecting with ringmaster
  // receive id and number of players
  int id;
  char player[32];
  memset(player, '\0', 32);
  recv(socket_fd, player, sizeof(player), 0);
  id = atoi(player);
  int num_players;
  char num[32];
  memset(num, '\0', 32);
  recv(socket_fd, num, sizeof(num), 0);
  num_players = atoi(num);
  printf("Connected as player %d out of %d total players\n", id, num_players);

  // build a port to listen to the neighbours
  int listen_fd;
  int listen_port = build_listen(&listen_fd);
  // send listening port to ringmaster
  char lport[32];
  memset(lport, '\0', 32);
  sprintf(lport, "%d", listen_port);
  send(socket_fd, lport, sizeof(port), 0); // send full
 
  // receive information of neighbour
  char neigh_port[32];
  char neigh_ip[32];
  memset(neigh_port, '\0', 32);
  memset(neigh_ip, '\0', 32);
  recv(socket_fd, neigh_port, 32, 0);
  recv(socket_fd, neigh_ip, 32, 0);

  //char* neigh_port_c;
  //sprintf(neigh_port_c, "%d", neigh_port);
  //request connect with former neighbour
  int neigh_fd = connect_neighbour(neigh_port, neigh_ip);
  //printf("connected with a neighbour\n");
  
  // accept connection request from next neighbour
  struct sockaddr_in neigh_addr;
  socklen_t addrlen = sizeof(neigh_addr);
  int fd = accept(listen_fd, (struct sockaddr *) &neigh_addr, &addrlen);
  if (fd == -1){
    error_message("Error: cannot accept connection on socket\n");
  }

  // tell ringmaster connecting neighbour successfully
  int flag = 1;
  char flag_message [8];
  memset(flag_message, '\0', 8);
  sprintf(flag_message, "%d", flag);
  send(socket_fd, flag_message, sizeof(flag_message), 0);

  // game begins
  potato_t potato;
  
  // select
  int max_fd = socket_fd;
  if (fd > max_fd){
    max_fd = fd;
  }
  if (neigh_fd > max_fd){
    max_fd = neigh_fd;
  }

  int player_receive_potato = 0;
  
  while(1){
    fd_set recv_fds;
    FD_ZERO(&recv_fds);
    //while(1){
    FD_SET(socket_fd, &recv_fds);
    FD_SET(fd, &recv_fds);
    FD_SET(neigh_fd, &recv_fds);
    
    select(max_fd+1, &recv_fds, NULL, NULL, NULL);
    size_t check_recv;
    if (FD_ISSET(socket_fd, &recv_fds)){
      //printf("\nreceive from ringmaster\n");
      check_recv = recv(socket_fd, &potato, sizeof(potato), MSG_WAITALL);
    }
    else if (FD_ISSET(fd, &recv_fds)){
      //printf("\nreceive from player %d\n", (id+1+num_players)%num_players);
      player_receive_potato += 1;
      check_recv = recv(fd, &potato, sizeof(potato), MSG_WAITALL);
    }
    else if (FD_ISSET(neigh_fd, &recv_fds)){
      //printf("\nreceive from player %d\n", (id-1+num_players)%num_players);
      player_receive_potato += 1;
      check_recv = recv(neigh_fd, &potato, sizeof(potato), MSG_WAITALL);
    }
    
    if(check_recv != sizeof(potato)){
      //printf("receive message not potato\n");
      //printf("size: %ld\n", check_recv);
      break;
    }
      
    if (potato.hops == 0 || potato.game_over_flag == 1){
      break;
    }
    
    potato.trace[potato.count] = id;
    potato.count += 1;
    
    //check
    //printf("current count: %d\n", potato.count);
    //printf("receive %d times potato\n", player_receive_potato);

    
    // last round
    if (potato.count == potato.hops){
      printf("I'm it\n");
      //printf("sizeof final potato: %ld\n", sizeof(potato));
      potato.game_over_flag = 1;
      send_full(socket_fd, &potato);

      break;
    }
    else{

      //printf("size of potato: %ld\n", sizeof(potato));
      
      srand ((unsigned int)(time(NULL)) + potato.count + num_players);
      int random = rand() % 2;
      if (random == 0){
	printf("Sending potato to %d\n", (id + 1 + num_players) % num_players);
	send_full(fd, &potato);
      }
      else{
	printf("Sending potato to %d\n", (id - 1 + num_players) % num_players);
	send_full(neigh_fd, &potato);
      }
    }
  }

  //printf("receive %d times potato\n", player_receive_potato);

  close(socket_fd);
  close(fd);
  close(neigh_fd);    
  freeaddrinfo(host_info_list);
  close(listen_fd);

  return EXIT_SUCCESS;
}

