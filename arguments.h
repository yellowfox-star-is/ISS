//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <stdbool.h>

int read_arguments(int argc, char* argv[], char **filename, char **hostname, bool *isServer, bool *isVerbose);
int verify_arguments(char* argv[], char *filename, char *hostname, bool isServer, bool isVerbose);

#endif //ARGUMENTS_H