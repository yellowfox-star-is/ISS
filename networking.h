//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3


#ifndef NETWORKING_H
#define NETWORKING_H

#include "smrcka_bat.h"

void *get_addr(struct sockaddr *sa);
int send_data(int socket, const struct addrinfo *serverinfo,unsigned char *data, int data_length);
int get_address_info(struct addrinfo *hints, struct addrinfo **serverinfo, char *hostname);
int initialize_socket(struct addrinfo *serverinfo);
int listen_for_packet();

extern bool packet_was_caught;
extern enum secret_protocol recognized_protocol;
extern char ip_src[CHAR_LIMIT];
extern char decrypted_packet[MAX_PACKET_LENGTH]; 

#endif //NETWORKING_H