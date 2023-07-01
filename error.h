//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

//=============================================
//as you can see, here I stole from myself
//but is it stealing, if it is my code?
//=============================================

// error.h
// Řešení IJC-DU1, příklad b), 19.3.2020
// Autor: Václav Sysel, FIT
// Přeloženo: gcc 7.5.0
//
// využito k projektu ISA
// drobné úpravy provedeny
// error.h version 2

#ifndef ERROR_H
#define ERROR_H

#include <stdlib.h>
void warning_msg(const char *fmt, ...);
void error_exit(int exit_code, const char *fmt, ...);
void printf_error(const char *fmt, ...);
#define ERR_OUT stderr

#endif //ERROR_H