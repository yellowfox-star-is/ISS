//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3

#include <openssl/asn1.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#include "error.h"
#include "smrcka_bat.h"
#include "client.h"
#include "networking.h"

enum client_state {START, SEND_HEADER, WAIT_FOR_HEADER, SEND_DATA, WAIT_FOR_ACCEPT, REPEAT_DATA, END, REPEAT_END, WAIT_FOR_FINISH, FREE, EXIT};

/*
 * Reads "count" of chars (bytes) from file handle and saves them to the pointer.
 * Function doesn't check if pointer is valid and can be dangerous to use.
 * Function returns number of chars that were read from the file handle.
 * If function got to the EOF, negative of number of characters is returned.
 */
int fill_buffer(FILE *file_input, unsigned char *buffer, int count)
{
    int c;
    int i = 0;

    while ((i < count) && (c = fgetc(file_input)) != EOF)
    {
        buffer[i] = (char) c;
        i++;
    }

    return c == EOF ? i * -1 : i;
}

/* Function that points to basename of filename char
 */
char *get_basename(char *filename)
{
    if (filename == NULL)
    {
        return NULL;
    }
    char * ptr = NULL;
    char * n_ptr = NULL;
    ptr = strchr(filename, '/');
    if (ptr == NULL)
    {
        return filename;
    }
    while (1)
    {
        ptr++;
        n_ptr = strchr(ptr, '/');
        if (n_ptr == NULL)
        {
            return ptr;
        }
        else
        {
            ptr = n_ptr;
        }
    }
}

/*
 * Function that contains the entirety of client.
 * The function structure is that of a state machine.
 * Function returns 0 if all went good and 1 if some error was encountered.
 * Client is heavily dependent on module networking, which is shared with Server.
 */
int start_client(char *filename, char *hostname, bool isVerbose)
{
    enum client_state state = START;
    int result = 0;

    struct addrinfo hints;
    struct addrinfo *serverinfo;
    int socket = 0;
    FILE *file_input = NULL;

    /* buffer is used caching of data read from file
     * the data are then given a prefix and info about their size
     * all data ready to be sent are saved in data variable
     */
    unsigned char buffer[MAX_DATA_LENGTH];
    int buffer_length = 0;
    unsigned char data[MAX_DATA_LENGTH];
    int data_length = 0;

    /*
     * client implements system of soft and hard "strikes" which keeps track of connectivity to the server
     * the server doesn't have this feature and only keeps receiving data
     * hard_strikes keep track of how manny "loops" the client had to repeat data no matter if it got response
     * soft_strikes keep track of how manny "loops" the client had to repeat data, without getting response
     */
    int soft_strikes = 0;
    int hard_strikes = 0;

    //state EXIT itself is a dummy state and program should never enter it via switch
    //if program runs correctly, the EXIT state is used for breaking the while loop
    while (state != EXIT)
    {
        if (cancel_received)
        {
            state = FREE;
        }
        switch (state)
        {
            /* START
             * initializes information needed for sending of files
             * this is th only part of application which can be force quited
             * any other part has file handle opened, so it must be ended via ending state FREE
             * START => SEND_HEADER
             */
            case START:
                result = get_address_info(&hints, &serverinfo, hostname);
                socket = initialize_socket(serverinfo);
                if (socket == -1)
                {
                    result = 1;
                    error_exit(1, "Program wasn't able to open a socket.\n");
                }
                file_input = fopen(filename, "r");
                if (file_input == NULL)
                {
                    result = 1;
                    error_exit(1, "Program wasn't able to open a file.\n");
                }
                if (isVerbose)
                {
                    printf("Client successfully started and initialized.\n");
                }
                state = SEND_HEADER;
            break;

            /* SEND_HEADER
             * client send header via format:
             * SECRET_START\n<filename>\n
             * the header is repeated until client receives reply from the server
             * SEND_HEADER => WAIT_FOR_HEADER
             */
            case SEND_HEADER:
                data_length = snprintf((char *)data, MAX_DATA_LENGTH, "SECRET_START\n%s\n", get_basename(filename));
                send_data(socket, serverinfo, data, data_length);
                if (isVerbose)
                {
                    printf("OUT: SEND_HEADER\n");
                }
                state = WAIT_FOR_HEADER;
            break;

            /* WAIT_FOR_HEADER, WAIT_FOR_ACCEPT, WAIT_FOR_FINISH
             * this design deviates from classic state machine
             * a lot of functions are redundant, so all three states are merged together
             * only difference is to which state will the machine return to
             * WAIT_FOR_HEADER => SEND_HEADER
             * WAIT_FOR_HEADER => SEND_DATA
             * WAIT_FOR_ACCEPT => REPEAT_DATA
             * WAIT_FOR_ACCEPT => SEND_DATA
             * WAIT_FOR_FINISH => REPEAT_END
             * WAIT_FOR_FINISH => FREE
             */
            case WAIT_FOR_HEADER:
            case WAIT_FOR_ACCEPT:
            case WAIT_FOR_FINISH:
                listen_for_packet();
                if (packet_was_caught) //TODO maybe add IP check
                {
                    if (recognized_protocol == SECRET_REPEAT)
                    {
                        //if repeat is received, function will fall through and return to repeat state

                        //program isn't moving forward but communication is still happening
                        //that is why soft_strikes are lowered
                        soft_strikes--;
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_REPEAT\n");
                        }
                    }
                    else if (recognized_protocol == SECRET_ACCEPT)
                    {
                        /*
                         * At this moment server replied, that data has been successfully received.
                         * The data_length must be cleared, data_length keeps track of how much data is in "buffer" data
                         * Clearing whole buffer would take too long, so only number of data in is cleared.
                         * Also the whole strike system is cleared here.
                         */
                        soft_strikes = 0;
                        hard_strikes = 0;
                        data_length = 0;
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_ACCEPT\n");
                        }

                        //In case communication took more attempts to reconnect, than is hard_strikes limit, user will
                        //be notified that the server is reachable again
                        if (hard_strikes >= STRIKE_LIMIT)
                        {
                            warning_msg("Server resumed communication.\n");
                        }
                        if (state == WAIT_FOR_FINISH)
                        {
                            printf("File '%s' has been sent to '%s'.\n", filename, hostname);
                        }
                        if (state == WAIT_FOR_HEADER)
                        {
                            printf("Sending file '%s' to '%s'.\n", filename, hostname);
                        }
                        state = state == WAIT_FOR_FINISH ? FREE : SEND_DATA;
                        break;
                    }
                    else if (recognized_protocol == SECRET_CORRUPTED)
                    {
                        if (isVerbose)
                        {
                            printf("IN:  SECRET_CORRUPTED???\n");
                        }
                    }
                }
                //Because manny states go into this case, program doesn't know, via which state it arrived.
                //The state which should be used need to be found out again
                if (state == WAIT_FOR_HEADER) state = SEND_HEADER;
                if (state == WAIT_FOR_ACCEPT) state = REPEAT_DATA;
                if (state == WAIT_FOR_FINISH) state = REPEAT_END;
                if (hard_strikes == STRIKE_LIMIT)
                {
                    //hard_strikes is added once more, so it doesn't continue to be incremented
                    hard_strikes++;
                    if (state == WAIT_FOR_FINISH)
                    {
                        warning_msg("Program didn't receive confirmation of receiving of file, but all data were sent.\n");
                        warning_msg("Data were probably send and the program will close itself.\n");
                    }
                    {
                        if (soft_strikes == STRIKE_LIMIT)
                        {
                            warning_msg("Program is stuck on repeating messages.\n");
                        }
                        else
                        {
                            warning_msg("Server stopped to communicate.\n");
                        }
                    }

                }
                /* increasing of strikes is skipped when state is WAIT_FOR_HEADER, because program at this moment
                 * is starting to communicate, and it would be unfair to count strikes when communication
                 * isn't established yet
                 */
                else if (state != WAIT_FOR_HEADER && hard_strikes < STRIKE_LIMIT)
                {
                    //each repeat of message is considered instability of communication
                    //that is why "strikes" are incremented
                    soft_strikes++;
                    hard_strikes++;
                }
            break;

            /* SEND_DATA
             * this is main part of the program
             * FILE_CHUNK of bytes is read from the file and saved into the buffer
             * into variable data is written prefix and then file data from the buffer
             * SEND_DATA => END
             * SEND_DATA => WAIT_FOR_ACCEPT
             *
             * REPEAT_DATA
             * this state is basically SEND_DATA but without the data filling part
             * data that are in the buffers from previous cycle will be used and sent again
             * REPEAT_DATA => WAIT_FOR_ACCEPT
             */
            case SEND_DATA:
                buffer_length = fill_buffer(file_input, buffer, FILE_CHUNK);
                /* In case fill_buffer returned ZERO or NEGATIVE number, it means that the file has been read whole
                 * In such case the program will swap into END state
                 */
                if (buffer_length <= 0)
                {
                    buffer_length = abs(buffer_length);
                    state = END;
                    break;
                }
                data_length += snprintf((char *)data, MAX_DATA_LENGTH, "SECRET_DATA\n%d\n", buffer_length);
                memcpy(data + data_length, buffer, buffer_length);
                data_length += buffer_length;
                __attribute__ ((fallthrough));
            case REPEAT_DATA:
                send_data(socket, serverinfo, data, data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_DATA\n");
                }
                state = WAIT_FOR_ACCEPT;
            break;

            /* END, REPEAT_END
             * state that is very similar to SEND_DATA but data are given a different prefix
             * in theory it could be used together with SEND_DATA but this has been denied for better readability
             * this function also incorporates REPEAT_END which is similar to REPEAT_DATA
             *
             * in case that you are going to change any of the two states
             * please check the other one, if it needs to be changed as well
             *
             * END => WAIT_FOR_FINISH
             * REPEAT_END => WAIT_FOR_FINISH
             */
            case END:
                data_length += snprintf((char *)data, MAX_DATA_LENGTH, "SECRET_END\n%d\n", buffer_length);
                memcpy(data + data_length, buffer, buffer_length);
                data_length += buffer_length;
                __attribute__ ((fallthrough));
            case REPEAT_END:
                send_data(socket, serverinfo, data, data_length);
                if (isVerbose)
                {
                    printf("OUT: SECRET_DATA\n");
                }
                state = WAIT_FOR_FINISH;
            break;

            /* FREE
             * Is a simple state which frees the filename
             * In case of any manually allocated buffers, they would be freed here.
             */
            case FREE:
                fclose(file_input);
                state = EXIT;
            break;

            /* In case any unrecognized state is used, the program will go into FREE state and safely quit.
             * This can happen if for example EXIT state is used, although that should never happen.
             */
            default:
                warning_msg("Implementation error. Client got into unrecognized state.\n");
                warning_msg("Please contact author.\n");
                result = 1;
                state = FREE;
            break;
        }
    }

    return result;
}
