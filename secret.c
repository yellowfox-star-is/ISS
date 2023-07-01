//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

//POSIX headers
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

//custom headers
#include "error.h"
#include "arguments.h"
#include "smrcka_bat.h"

//two sides of the program
//each "mode" of the program is separated into separate module
#include "client.h"
#include "server.h"

/*
Handler for that is called when user presses CTRL-C.
Function notifies user, because it can take a moment before server/client module timeouts.
Handler sets cancel_received to true, server and client module check for this variable
and appropriately shut down.
*/
void intHandler(int dummy)
{
    (void)dummy;
    fprintf(ERR_OUT, "\n");
    warning_msg("Interrupt received. Program will close shortly.\n");
    cancel_received = true;
}

int main (int argc, char* argv[])
{
    char *filename = NULL;
    char *hostname = NULL;
    bool isServer = false;
    bool isVerbose = false;

    read_arguments(argc, argv, &filename, &hostname, &isServer, &isVerbose);
    verify_arguments(argv, filename, hostname, isServer, isVerbose);

    signal(SIGINT, intHandler);

    if (isServer)
    {
        return start_server(isVerbose);
    }
    else
    {
        return start_client(filename, hostname, isVerbose);
    }

    return 0;
}