//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#ifndef SMRCKA_BAT_H
#define SMRCKA_BAT_H

#include <stdbool.h>
//contains global constants and macros

#define CHAR_LIMIT 100
extern const char indent[5];
extern const unsigned char key_128[17];
#define MAX_PACKET_LENGTH 1500
#define FILE_CHUNK 900
#define MAX_DATA_LENGTH 1000
#define MAX_ENCRYPTED_DATA_LENGTH 1300
enum secret_protocol {SECRET_START, SECRET_DATA, SECRET_END, SECRET_ACCEPT, SECRET_REPEAT, SECRET_CORRUPTED};
#define ALLOWED_CORRUPTION 2
#define TIMEOUT (5)
extern bool replace_file;
extern bool cancel_received;
extern bool isLoopback;
#define STRIKE_LIMIT (20)

#define ICMPV6_CHECKSUM 0

#endif //SMRCKA_BAT_H

#if 0
name of this file is rather confusing, so I will explain
In the time of writing of this file, Ing. Aleš Smrčka Ph.D. was teacher one of my lectures that I had the privilege to attend during my studies on VUT FIT in Brno.
He adviced us to always contain "magical numbers" in some constants or macros or other definitions and use those instead, for better readability of the code
and for easier editation later on. He also told us and now I will be paraphrasing. To always imagine him standing behind us with a baseball bat
if we were to write a magical number into a code. I contemplated for many hours how to name a file which would contain such constants and thus I concluded
that I will name it in his honor and in honor of emoji that was created on school discord server.
:smrckaBat:
#endif