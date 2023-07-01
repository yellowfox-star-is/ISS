//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include "error.h"
#include "smrcka_bat.h"
#include "networking.h"

enum server_state {WAIT_FOR_HEADER, REPEAT_HEADER, ACCEPT_HEADER, WAIT_FOR_DATA, REPEAT_DATA, ACCEPT_DATA, ACCEPT_END, FINISH, CANCEL, EXIT};

/* Own implementation of strncpy
 * strncpy stopts when \0 is used, but my protocol uses \n as ends for data, so custom strncpy had to be implemented
 * copies data from target to source until \n or \0 is encountered
 * the implementation is heavily inspired by sample implementation in Linux Programmer's Manual
 * functions returns 0 if all went good 1 if an error happened
 */
int copy_something(char *target, char *source, int max_length)
{
    char *ptr = NULL;
    if ((ptr = strchr(source, '\n')) != NULL)
    {
        int i;
        ptr += 1;
        for (i = 0 ; i < max_length && ptr[i] != '\n' && ptr[i] != '\0' ; i++)
        {
            target[i] = ptr[i];
        }
        for (;i < max_length; i++)
        {
            target[i] = '\0';
        }

        return 0;
    }
    return 1;
}

/* Wrapper function for previous copy_something
 * this one is used for copying filename from received header
 */
int copy_filename(char *target, char *source, int max_length)
{
    if (strcmp(source, "SECRET_START\n"))
    {
        return copy_something(target, source, max_length);
    }
    return 1;
}

/* Wrapper function for previous copy_something
 * this one is used for copying data length from received packet
 */
int copy_length(char *source)
{
    char tmp[CHAR_LIMIT];
    if (strcmp(source, "SECRET_DATA\n") || strcmp(source, "SECRET_END\n"))
    {
        if (!copy_something(tmp, source, CHAR_LIMIT))
        {
            return atoi(tmp);
        }
    }
    return -1;
}

/* Function for finding a free filename.
 * In case a filename is used this function can be used to generate an unused filename.
 * returns 0 and replaces filename if found
 * returns 1 if not found
 */
int change_filename(char *filename)
{
    char new_filename[CHAR_LIMIT];
    char new_end[CHAR_LIMIT];
    for (unsigned int i = 0; i < UINT_MAX; i++)
    {
        strncpy(new_filename, filename, CHAR_LIMIT);
        snprintf(new_end, CHAR_LIMIT, ".%d", i);
        strncat(new_filename, new_end, CHAR_LIMIT);
        if (!access(new_filename, F_OK))
        {
            strcpy(filename, new_filename);
            return 0;
        }
    }
    warning_msg("An alternative filename couldn't be found.\n");
    return 1;
}

/* writes "count" of chars (bytes) from stream into file handle
 * returns 0 if all data has been copied, returns -1 if an error happened
 * can be easily edited to return number of bytes copied
 */
int write_to_file(FILE *file, char *stream, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (fprintf(file, "%c", stream[i]) <= 0)
        {
            warning_msg("Failed writing to file.\n");
            return -1;
        }
    }
    return 0;
}

//taken from: https://stackoverflow.com/questions/46070363/how-to-use-strchr-multiple-times-to-find-the-nth-occurrence/46070740
const char* nth_strchr(const char* s, int c, int n)
{
    int c_count;
    char* nth_ptr;

    for (c_count=1,nth_ptr=strchr(s,c); 
        nth_ptr != NULL && c_count < n && c!=0; 
        c_count++)
    {
        nth_ptr = strchr(nth_ptr+1, c);
    }

    return nth_ptr;
}

/*
 * Function that contains the entirety of server.
 * The function structure is that of a state machine.
 * Function returns 0 if all went good and 1 if some error was encountered.
 * Server is heavily dependent on module networking, which is shared with Client.
 */
int start_server(bool isVerbose)
{
    //server state
    int result = 0;
    enum server_state state = WAIT_FOR_HEADER;

    //file info
    FILE *file = NULL;
    char filename[CHAR_LIMIT];
    filename[0] = '\0';

    //client info
    char clientname[CHAR_LIMIT];
    clientname[0] = '\0';

    //networking
    int socket = 0;
    struct addrinfo hints;
    struct addrinfo *serverinfo;

    // variables for data manipulation
    int out_data_length = 0;
    unsigned char out_data[MAX_DATA_LENGTH];

    int new_in_data_length = 0;
    int in_data_length = 0;
    char *in_data;

    //state EXIT itself is a dummy state and program should never enter it via switch
    //if program runs correctly, the EXIT state is used for breaking the while loop
    while (state != EXIT)
    {
        if (cancel_received)
        {
            state = CANCEL;
        }
        switch (state)
        {
            /* WAIT_FOR_HEADER
             * opening state for server, server cycles on this state, until a header packet is received
             * server can accept only one file from one client at a time
             * if this was to be upgraded, program could be forked when header caught and initialization started
             * WAIT_FOR_HEADER => REPEAT_HEADER
             * WAIT_FOR_HEADER => ACCEPT_HEADER
             *
             * WARNING: server uses a lot of variables, that are automatically allocated, they would have to be edited
             * and probably manually allocated
             */
            case WAIT_FOR_HEADER:
                listen_for_packet();
                if (packet_was_caught)
                {
                    if (recognized_protocol == SECRET_CORRUPTED)
                    {
                        /* if socket was initialized, send repeat
                         * this can in theory never happen, but it doesn't matter, because in default state
                         * client keeps sending headers until server responds
                         * program can be easily modified to initialize socket even if protocol was corrupted,
                         * but it is safer like this
                         */
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_CORRUPTED ???\n");
                        }
                        if (socket != 0) 
                        {
                            state = REPEAT_HEADER;
                        }
                        break;
                    }
                    /* only when server receives header it can start to initialize variables
                     */
                    if (recognized_protocol == SECRET_START)
                    {
                        //HERE: this would be the place to fork program
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_START\n");
                        }

                        //initialization
                        strcpy(clientname, ip_src);
                        if (get_address_info(&hints, &serverinfo, clientname))
                        {
                            error_exit(1, "Couldn't resolve clients name.\n");
                        }
                        socket = initialize_socket(serverinfo);
                        if (socket == -1) //if socket cannot be opened
                        {
                            error_exit(1, "Program wasn't able to open a socket.\n");
                        }
                        if (copy_filename(filename, decrypted_packet, CHAR_LIMIT))
                        {
                            error_exit(1, "Filename is bigger than limit of program.\n");
                        }
                        if (!access(filename, F_OK)) //If filename already exists
                        {
                            warning_msg("File name already exists.\n");
                            if (replace_file)
                            {
                                warning_msg("Original file will be replaced.\n");
                            }
                            else
                            {
                                warning_msg("Another name will be used.\n");
                                if (change_filename(filename))
                                {
                                    exit(1);
                                }
                                warning_msg("File will be saved as: %s\n", filename);
                            }
                        }
                        file = fopen(filename, "w");
                        if (file == NULL)
                        {
                            error_exit(1, "Program wasn't able to open a file.\n");
                        }
                        state = ACCEPT_HEADER;
                        break;
                    }
                    if (isVerbose)
                    {
                        printf("Unexpected secret packet received. Was waiting for header.\n");
                    }
                }
            break;

            /* WAIT_FOR_DATA
             * main part of the server, here are data received and saved, other states are merely different responses
             * for the client and keep track of how far did the program got, this is state that does the heavy lifting
             * WAIT_FOR_DATA => ACCEPT_HEADER
             * WAIT_FOR_DATA => REPEAT_DATA
             * WAIT_FOR_DATA => CANCEL
             * WAIT_FOR_DATA => ACCEPT_DATA
             * WAIT_FOR_DATA => ACCEPT_END
             */
            case WAIT_FOR_DATA:
                listen_for_packet();
                if (packet_was_caught && !strcmp(clientname, ip_src))
                {
                    if (recognized_protocol == SECRET_CORRUPTED)
                    {
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_CORRUPTED ???\n");
                        }
                        state = REPEAT_DATA;
                        break;
                    }
                    if (recognized_protocol == SECRET_START)
                    {
                        /* we will presume that:
                         *    the filename is same
                         *    the client didn't get accept
                         *    and that is why they are sending header again
                         *    program doesn't check if that is actually truth
                         *    in theory client could have started another file transaction
                         *
                         *    WARNING!!!
                         *    this can be dangerous! if packet with header was to be lost and client meanwhile
                         *    sent header again and some data, server would respond to the late header but client
                         *    would think that the server is responding to data
                         */
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_HEADER\n");
                        }
                        state = ACCEPT_HEADER;
                        break;
                    }
                    if (recognized_protocol == SECRET_DATA || recognized_protocol == SECRET_END)
                    {
                        if (isVerbose)
                        {
                            if (recognized_protocol == SECRET_DATA)
                            {
                                printf("IN:  SECRET_DATA\n");
                            }
                            else if (recognized_protocol == SECRET_END)
                            {
                                printf("IN:  SECRET_END\n");
                            }
                        }
                        new_in_data_length = copy_length(decrypted_packet);
                        if (new_in_data_length == 0)
                        {
                            state = REPEAT_DATA;
                            break;
                        }
                        if (new_in_data_length == in_data_length)
                        {
                            /* we cannot tell now, if the packets are same or not
                             * one way would be to implement timestamps, but because of time press
                             * they aren't implemented
                             */
                        }
                        in_data_length = new_in_data_length;
                        /* now we need to set pointer to the start of the data, in order to do that, we need to skip
                         * two \n that are in the header
                         */
                        if ((in_data = (char *)(nth_strchr(decrypted_packet, '\n', 2))) == NULL)
                        {
                            if (isVerbose)
                            {
                                warning_msg("Received invalid data.\n");
                                state = REPEAT_DATA;
                                break;
                            }
                        }
                        /* the nth_strchr points to nth found \n in variable
                         * this means that we are now pointing on the \n character and not on the start of the data,
                         * so we need to increment the pointer to get behind \n
                         */
                        in_data++;
                        /* now we can safely dump data into the file */
                        if (write_to_file(file, in_data, in_data_length) < 0)
                        {
                            state = CANCEL;
                            break;
                        }
                        state = recognized_protocol == SECRET_DATA ? ACCEPT_DATA : ACCEPT_END;
                        break;
                    }
                }
            break;

            /* following 5 states: REPEAT_HEADER, ACCEPT_HEADER, REPEAT_DATA, ACCEPT_DATA, ACCEPT_END
             * are very similar, they all just send a response back to the client and then change into another state
             * that waits for the response from client, they could be modified into one or two states but
             * the surrounding structures would be more complicated than the states themselves
             * the states are also build from very basic functions and so are allowed to survive
             */
            //REPEAT_HEADER => WAIT_FOR_HEADER
            case REPEAT_HEADER:
                out_data_length = snprintf((char *)out_data, MAX_DATA_LENGTH, "SECRET_REPEAT\n");
                send_data(socket, serverinfo, out_data, out_data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_REPEAT\n");
                }
                state = WAIT_FOR_HEADER;
                break;
            break;

            //ACCEPT_HEADER => WAIT_FOR_DATA
            case ACCEPT_HEADER:
                out_data_length = snprintf((char *)out_data, MAX_DATA_LENGTH, "SECRET_ACCEPT\n");
                send_data(socket, serverinfo, out_data, out_data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_ACCEPT\n");
                }
                state = WAIT_FOR_DATA;
                break;
            break;

            //REPEAT_DATA => WAIT_FOR_DATA
            case REPEAT_DATA:
                out_data_length = snprintf((char *)out_data, MAX_DATA_LENGTH, "SECRET_REPEAT\n");
                send_data(socket, serverinfo, out_data, out_data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_REPEAT\n");
                }
                state = WAIT_FOR_DATA;
                break;
            break;

            //ACCEPT_DATA => WAIT_FOR_DATA
            //ACCEPT_END => FINISH
            case ACCEPT_DATA:
            case ACCEPT_END:
                out_data_length = snprintf((char *)out_data, MAX_DATA_LENGTH, "SECRET_ACCEPT\n");
                send_data(socket, serverinfo, out_data, out_data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_ACCEPT\n");
                }
                state = state == ACCEPT_DATA ? WAIT_FOR_DATA : FINISH;
            break;

            /* FINISH
             * all data are now received, info to client has been sent and there is nothing to do but end
             * FINISH => EXIT
             */
            case FINISH:
            printf("File '%s' has been received.\n", filename);
                fclose(file);
                state = EXIT;
                break;
            break;

            /* EXIT state is a dummy state and should never be entered
             * but if that were to happen, the program will go into CANCEL state and safely quit.
             * EXIT => CANCEL
             */
            case EXIT:
                warning_msg("Implementation error. Server got into unrecognized state.\n");
                warning_msg("Please contact author.\n");
                result = 1;
                state = CANCEL;
            break;

            /* CANCEL
             * this state is an error state that is entered, if something bad happens
             * if file exists it will be closed and removed
             */
            case CANCEL:
                result = 1;
                if (file != NULL)
                {
                    fclose(file);
                }
                if (filename[0] != '\0')
                {
                    remove(filename);
                }
                state = EXIT;
            break;
        }
    }
    return result;
}