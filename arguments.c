//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#include "error.h"
#include "bit.h"
#include "networking.h"
#include "arguments.h"
#include "smrcka_bat.h"

/*
Parses arguments using getopt and returns variables in pointers.
*/
int read_arguments(int argc, char* argv[], char **filename, char **hostname, bool *isServer, bool *isVerbose)
{
    /*
    Opterr is saved and returned on the end of the function.
    */
    int original_opterr = opterr;
    opterr = 0;

    //initialization of variables
    *filename = NULL;
    *hostname = NULL;
    *isServer = false;
    *isVerbose = false;
    isLoopback = false;

    int result = 0;
    int c;

    while ((c = getopt(argc, argv, "r:s:lvo")) != -1)
        switch (c)
        {
            case 'r':
                *filename = optarg;
                break;
            case 's':
                *hostname = optarg;
                break;
            case 'l':
                *isServer = true;
                break;
            case 'v':
                *isVerbose = true;
                break;
            case 'o':
                isLoopback = true;
                break;

            case '?':
                switch (optopt)
                {
                    case 'r':
                        warning_msg("Option -r <file> requires an argument.\n");
                        break;
                    case 's':
                        warning_msg("Option -s <ip|hostname> requires an argument.\n");
                        break;
                    default:
                        warning_msg("Unknown option '-%c' will be skipped.\n", optopt);
                        break;
                }
                result = 1;
                break;
            default:
                error_exit(1, "Critical error happened during argument parsing.\n");
                continue;
        }

    opterr = original_opterr;
    return result;
}

int verify_filename(char *filename, bool isVerbose);
int verify_hostname(char *hostname, bool isVerbose);
/*
 * Verifies, if passed arguments are usable for a program. Exits the program if arguments are unusable.
 */
int verify_arguments(char* argv[], char *filename, char *hostname, bool isServer, bool isVerbose)
{
    int result = 0;

    //No arguments were recognized so the program prints basic help and ends.
    if (filename == NULL && hostname == NULL && isServer == false)
    {
        printf("Usage:\n");
        printf("%s -r <file> -s <ip|hostname> [-l]\n", argv[0]);
        printf("%s-r <file> : name of file to be transmitted\n", indent);
        printf("%s-s <ip|address : ip address/hostname of server\n", indent);
        printf("%s-l : program will start in a server mode\n", indent);
        exit(0);
    }

    if (isVerbose)
    {
        printf("The program was started in a verbose mode. Prepare, that it will be unnecessarily chatty.\n");
        printf("Received arguments:\n");
        printf("%sfilename: %s\n",indent, filename);
        printf("%shostname: %s\n",indent, hostname);
        printf("%sisServer: %s\n",indent, BOOL2STRING(isServer));
        printf("%sisVerbose: %s\n",indent, BOOL2STRING(isVerbose));
    }

    if (isServer)
    {
        if (isVerbose)
        {
            char state = 0;
            if (filename != NULL) BIT_SET(state, 0);
            if (hostname != NULL) BIT_SET(state, 1);
            if (state)
            {
                printf("Received unnecessary arguments:\n");
                if (BIT_GET(state, 0)) printf("    filename: %s\n", filename);
                if (BIT_GET(state, 1)) printf("    ip|hostname: %s\n", hostname);
                printf("These arguments won't be used and checked, as they are not needed for server mode.\n");
            }
        }
        return 0;
    }
    else //isClient
    {
        /*
         * Results of verifying functions are saved via OR assignment.
         * If normal assignment was to be used, verify_filename is technically never used,
         * because checking of hostname would always replace the result and higher optimization could skip
         * verify_filename, but it must be run, because it writes information for user during checkup.
         */
        if (filename != NULL) //file verification
        {
            result |= verify_filename(filename, isVerbose);
        }
        else
        {
            warning_msg("File argument not received.\n");
            result |= 1;
        }

        if (hostname != NULL) //hostname verification
        {
            result |= verify_hostname(hostname, isVerbose);
        }
        else
        {
            warning_msg("Hostname argument not received.\n");
            result |= 1;
        }
    }

    if (result)
    {
        //If any of the arguments aren't usable, program cannot start and thus the program will end itself.
        error_exit(result, "Program wasn't able to use passed arguments.\n");
    }

    return result;
}

int verify_filename(char *filename, bool isVerbose)
{
    int result = 0;

    if (access(filename, F_OK))
    {
        warning_msg("File \"%s\" doesn't exists.\n", filename);
        result = 1;
    }
    else
    {
        if (isVerbose)
        {
            printf("File \"%s\" exists.\n", filename);
        }
        if (access(filename, R_OK))
        {
            warning_msg("File \"%s\" cannot be accessed for reading.\n", filename);
            result = 1;
        }
        else
        {
            if (isVerbose)
            {
                printf("File \"%s\" can be accessed for reading.\n", filename);
            }
        }
    }
    return result;
}

int verify_hostname(char *hostname, bool isVerbose)
{
    int result = 0;
    int getadd_result = 0;

    struct addrinfo hints; //prepares structs for function get_address_info
    struct addrinfo *serverinfo;

    getadd_result = get_address_info(&hints, &serverinfo, hostname);

    if (getadd_result != 0)
    {
        warning_msg("Couldn't verify hostname: %s\n    %s\n",hostname, gai_strerror(getadd_result));
        result = 1;
    }
    else if (isVerbose)
    {
        printf("Verified that hostname \"%s\" can be used.\n", hostname);
        char ip[CHAR_LIMIT];
        inet_ntop(serverinfo->ai_family, get_addr(serverinfo->ai_addr), ip, CHAR_LIMIT);
        printf("Translated hostname to: %s\n", ip);
    }

    return result;
}