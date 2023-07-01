//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#include "smrcka_bat.h"
#include <openssl/aes.h>
#include <stdbool.h>
#include <string.h>

const char indent[5] = "    ";
unsigned const char key_128[17] = "xsysel09\0\0\0\0\0\0\0\0\0";
bool replace_file = true;
bool cancel_received = false;
bool isLoopback = false;
