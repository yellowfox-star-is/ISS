//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#ifndef BIT_H
#define BIT_H

#define BIT_SET(target, position) target |= 1 << position
#define BIT_CLR(target, position) target &= ~(1 << position)
#define BIT_FLP(target, position) target ^= 1 << position
#define BIT_GET(target, position) (target & (1 << position)) >> position
#define BOOL2STRING(input) (input ? "true" : "false")

#endif //ERROR_H