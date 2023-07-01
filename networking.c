//author: Václav Sysel
//VUT FIT, 5. semestr
//ISA, projekt
//varianta: skrytý kanál
//
//feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
//my style of coding is fairly specific and I have no doubt, that you could be found out for it
//so... let be inspired but no steal steal <3
//also also, you would be 100% found for this mess of headers that I have here

#include <net/ethernet.h>
#include <net/if_arp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <pcap.h>
#include <pcap/pcap.h>
#include <errno.h>
#define __FAVOR_BSD          // important for tcphdr structure
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h> 
#include <err.h>

#ifdef __linux__            // for Linux
#include <netinet/ether.h> 
#include <time.h>
#include <pcap/pcap.h>
#endif

#include <netinet/icmp6.h>
#include <netinet/ip6.h>

#include "error.h"
#include "networking.h"
#include "smrcka_bat.h"

//number of seconds how long will PCAP wait before timeout
#define PCAP_TIMEOUT 5
//Constants for sizes of different headers
#define SIZE_ETHERNET (sizeof(struct ether_header))
#define SIZE_IPV4 (sizeof(struct iphdr))
#define SIZE_IPV6 (sizeof(struct ip6_hdr))
#define SIZE_ICMP (sizeof(struct icmphdr))
#define SIZE_ICMPV6 (sizeof(struct icmp6_hdr))

//Macro for moving pointer on packet behind header
#define DROP_HEADER(prev_hdr, size) (((unsigned char *)prev_hdr) + size)

// packet capture handle
pcap_t *handle;

//variables accessible for packet catching
bool packet_was_caught = false;
enum secret_protocol recognized_protocol;
char ip_src[CHAR_LIMIT];
char decrypted_packet[MAX_PACKET_LENGTH];

/* Functions that opens sockaddr and returns correct address type
 * returns pointer to address if found, NULL if not
 */
void *get_addr(struct sockaddr *sa)
{
    if (sa == NULL)
    {
        return NULL;
    }

    if (sa->sa_family == AF_INET) //is ipv4
    {
        return &(((struct sockaddr_in *) sa ) -> sin_addr);
    }
    else if (sa->sa_family == AF_INET6) //is ipv6
    {
        return &(((struct sockaddr_in6 *) sa ) -> sin6_addr);
    }

    return NULL;
}

//Simple wrapper function for function socket
//Always initializes socket for sending ICMP or ICMPv6  ECHO_REQUEST
int initialize_socket(struct addrinfo *serverinfo)
{
    int protocol = serverinfo->ai_family == AF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6;
	return socket(serverinfo->ai_family, serverinfo->ai_socktype, protocol);
}

/* Resolves hostname using getaddrinfo
 * it is basically a wrapper function
 */
int get_address_info(struct addrinfo *hints, struct addrinfo **serverinfo, char *hostname)
{
    //replaces all bytes in struct hints with zeroes
    //it is from strings.h but it works nicely in this situation
    memset(hints, 0, sizeof(*hints));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_RAW;
    return getaddrinfo(hostname, NULL, hints, serverinfo);
}

//handleErrors taken from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
int handleErrors(void)
{
    ERR_print_errors_fp(ERR_OUT);
    return EXIT_FAILURE;
}

//encrypt taken from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        return handleErrors();

    /*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key_128, NULL))
        return handleErrors();

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        return handleErrors();
    ciphertext_len = len;

    /*
     * Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        return handleErrors();
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

//encrypt taken from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int plaintext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key_128, NULL))
        handleErrors();

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary.
     */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors();
    plaintext_len = len;

    /*
     * Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
        handleErrors();
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}

/* Taken from:
 * https://www.mycplus.com/source-code/c-source-code/ping/
 * calculates ICMPv4 checksum
 */
unsigned short checksum(unsigned short *buffer, int size)
{
	unsigned long cksum = 0;
 
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(unsigned short);
	}
	if (size)
	{
		cksum += *(unsigned char*)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (unsigned short)(~cksum);
}

#if ICMPV6_CHECKSUM
/* Function that adds IPv6 pseudo header and calculates cheksum for ICMPv6
 *
 * WARNING: at the time of writing function wasn't finished!
 * thus ICMPv6 checksum was disabled with macro ICMPV6_CHECKSUM! (contained in file smrcka_bat.h)
 * if the macro is set to zero, chances are that the checksum isn't finished, please don't turn on
 */
unsigned short checksum_v6(unsigned short *buffer, int size)
{
    //preparation of pseudo header
    char pseudo_packet[MAX_PACKET_LENGTH];
    memset(pseudo_packet, 0, MAX_PACKET_LENGTH);
    memcpy(pseudo_packet, src_ip, sizeof(unsigned short));
    memcpy(pseudo_packet + sizeof(unsigned short), dst_ip, sizeof(unsigned short));
    
    return checksum(pseudo_packet);
}
#endif

/* Function that encrypts data and sends them using predefined socket
 * returns 0 if all went good and 1 if an error happened
 */
int send_data(int socket, const struct addrinfo *serverinfo,unsigned char *data, int data_length)
{
    //variables initialization
    unsigned char packet[MAX_PACKET_LENGTH];
    unsigned char encrypted_data[MAX_ENCRYPTED_DATA_LENGTH];
    int encrypted_data_length = 0;

    // This state should never happen so in case it does, I wanna know about it.
    if (data_length < 0 || data_length >= MAX_DATA_LENGTH)
    {
        warning_msg("Implementation error. Failed during packet preparation. Data too large.\n");
        warning_msg("Please contact author.\n");
        return 1;
    }


    encrypted_data_length = encrypt(data, data_length, encrypted_data);
    // This state should never happen as well... so in case it doooes, I wanna know about it as well ;)
    if (encrypted_data_length >= MAX_ENCRYPTED_DATA_LENGTH)
    {
        warning_msg("Implementation error. Failed during packet preparation. Encrypted data too large.\n");
        warning_msg("Please contact author.\n");
        return 1;
    }

    //packet preparation
    memset(&packet, 0, MAX_PACKET_LENGTH);
    //makes pointer to packet, so it can be manipulated with better
    struct icmphdr *icmp_header = (struct icmphdr *)packet;
    //header for icmp and icmpv6 is same, the only different is checksum and message type numbers
    //this takes care of the latter
    icmp_header->type = serverinfo->ai_family == AF_INET ? ICMP_ECHO : ICMP6_ECHO_REQUEST;

    memcpy(packet + sizeof(struct icmphdr), encrypted_data, encrypted_data_length);

    /* checksum works correctly, but checksum for icmpv6 isn't implemented in the moment of writing
     * (just in case I forgot to remove this note, please check value in macro ICMPV6_CHECKSUM)
     * OS will replace incorrect ICMPV6 header with correct one, so the packet will send but this program cannot
     * check validity of ICMPV6 checksums, which is shame but it will work even without it
     */
    icmp_header->checksum = checksum((unsigned short *)icmp_header, sizeof(struct icmphdr) + encrypted_data_length);

    if (sendto(socket, packet, sizeof(struct icmphdr) + encrypted_data_length, 0, (struct sockaddr *)(serverinfo->ai_addr), serverinfo->ai_addrlen) < 0)
    {
        warning_msg("Program wasn't able to send packet.\n");
        return 1;
    }

    return 0;
}

/* Custom comparer for strings
 * takes two strings and length to compare
 * returns number of different characters
 * that means:
 * returns 0 if strings are same
 * returns number of different characters if different
 */
int string_cmp_n(char *str1, const char *str2, int length)
{
    int result = 0;
    for (int i = 0; i < length; i++)
    {
        result += str1[i] == str2[i] ? 0 : 1;
    }
    return result;
}

/* Comparer function for "SECRET PROTOCOL"
 * Compares starts of data with known SECRET headers
 * takes pointer to a string
 * returns appropriate secret enum if found out
 * returns SECRET_CORRUPTED if sufficient partial match (can be set with ALLOWED_CORRUPTION)
 * returns -1 if no resemblance with SECRET protocol is found
 */
int is_secret(char* data)
{
    const char s[] = "SECRET";
    const int s_len = strlen(s);
    const char s_start[] = "SECRET_START";
    const int s_start_len = strlen(s_start);
    const char s_data[] = "SECRET_DATA";
    const int s_data_len = strlen(s_data);
    const char s_end[] = "SECRET_END";
    const int s_end_len = strlen(s_end);
    const char s_repeat[] = "SECRET_REPEAT";
    const int s_repeat_len = strlen(s_repeat);
    const char s_accept[] = "SECRET_ACCEPT";
    const int s_accept_len = strlen(s_accept);

    int corruption = string_cmp_n(data, s, s_len);
    if (corruption <= ALLOWED_CORRUPTION)
    {
        if (corruption != 0) return SECRET_CORRUPTED;
        if (!string_cmp_n(data, s_start, s_start_len)) return SECRET_START;
        if (!string_cmp_n(data, s_data, s_data_len)) return SECRET_DATA;
        if (!string_cmp_n(data, s_end, s_end_len)) return SECRET_END;
        if (!string_cmp_n(data, s_repeat, s_repeat_len)) return SECRET_REPEAT;
        if (!string_cmp_n(data, s_accept, s_accept_len)) return SECRET_ACCEPT;
        return SECRET_CORRUPTED;
    }

    return -1;
}

/* Simple alarm handler for breaking pcap_loop so program can check up on it's state
 * This program is only one thread one, so it needs to break from the loop to check on rest of it's state
 */
void alarm_handler(int sig)
{
    (void)sig;
    pcap_breakloop(handle);
}

/* The bread and butter of receiving of files
 * handler of pcap_loop verifies that the packet is valid and if it is, it breaks pcap_loop and returns
 * program control back to appropriate module which called listen for packet
 * program is written in a similar manner, once for IPv4 and second for IPv6
 * all useful data are written into global variables, which are then accessed in other modules
 * variables in question:
 *     bool packet_was_caught;
 *     enum secret_protocol recognized_protocol;
 *     char ip_src[CHAR_LIMIT];
 *     char decrypted_packet[MAX_PACKET_LENGTH];
 */
void mypcap_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    (void)args;

    struct ether_header *eptr;      // pointer to the beginning of Ethernet header

    struct ip *my_ip;               // pointer to the beginning of IP header
    struct icmphdr *icmpv4hdr;
  
    struct ip6_hdr *my_ipv6;
    struct icmp6_hdr *icmpv6hdr;

    unsigned char *encrypted_data;
    unsigned char data[MAX_DATA_LENGTH];
    int possibly_caught = 0;
    //secret_protocol is an enum, so it goes from 0 on, so if we want to tell that the protocol isn't, it needs to be -1
    int secret_protocol = -1;
    unsigned short old_checksum = 0;
    unsigned short new_checksum = 0;
    /* corrupting as a mechanism that allows partial match to happen
     * if program finds a match but checksum isn't valid, the data in question are probably unusable
     * bool corrupting than sets protocol to SECRET_CORRUPTED and the program the knows to ask for
     * packet once more
     */
    bool corrupting = false;

    int data_size = header->len;
    //saves pointer to ethernet header
    eptr = (struct ether_header *) packet;
    switch (ntohs(eptr->ether_type))
    {
        case ETHERTYPE_IP:
            //saves pointer to ipv4 header "drops ethernet header"
            my_ip = (struct ip*)DROP_HEADER(packet, SIZE_ETHERNET);
            data_size -= SIZE_ETHERNET;
            if (my_ip->ip_p == 1) //if is ICMP
            {
                //drops ipv4 header
                icmpv4hdr = (struct icmphdr*)DROP_HEADER(my_ip, SIZE_IPV4);
                data_size -= SIZE_IPV4;
                if (icmpv4hdr->type == ICMP_ECHO)
                {
                    //verifies that the checksum is correct
                    old_checksum = icmpv4hdr->checksum;
                    icmpv4hdr->checksum = 0;
                    new_checksum = checksum((unsigned short *)icmpv4hdr, data_size);
                    if (old_checksum != new_checksum)
                    {
                        corrupting = true;  
                    }

                    //decrypts data
                    encrypted_data = DROP_HEADER(icmpv4hdr, SIZE_ICMP);
                    data_size -= SIZE_ICMP;
                    data_size = decrypt(encrypted_data, data_size, data);

                    /* At this moment we cannot verify if the data is correct because we would have to do it two times
                     * Once for IPV4 and once for IPV6 so instead only a variable is set, that there is a possible catch
                     */
                    possibly_caught = 1;
                }
            }
        break;

        case ETHERTYPE_IPV6:
            //drops ethernet header
            my_ipv6 = (struct ip6_hdr*)DROP_HEADER(packet, SIZE_ETHERNET);
            data_size -= SIZE_ETHERNET;
            if (my_ipv6->ip6_nxt == 58) //if is ICMPv6
            {
                //drops ipv6 header
                icmpv6hdr = (struct icmp6_hdr*)DROP_HEADER(my_ipv6, SIZE_IPV6);
                data_size -= SIZE_IPV6;
                if (icmpv6hdr->icmp6_type == ICMP6_ECHO_REQUEST)
                {
                    /* ICMPV6 checking of checksum isn't implemented at the time of writing
                     * please check macro ICMPV6_CHECKSUM to know for sure
                     * it is a shame, that the file can't check if integrity of files is intact, but because of time
                     * press it couldn't be implemented.
                     */
                    #if ICMPV6_CHECKSUM
                    old_checksum = icmpv6hdr->icmp6_cksum;
                    icmpv6hdr->icmp6_cksum = 0;
                    new_checksum = checksum_v6((unsigned short *)icmpv6hdr, data_size);
                    if (old_checksum != new_checksum)
                    {
                        corrupting = true;
                    }
                    #endif

                    //drops icmpv6 header
                    encrypted_data = DROP_HEADER(icmpv6hdr, SIZE_ICMPV6);
                    data_size -= SIZE_ICMPV6;

                    //decrypts data
                    data_size = decrypt(encrypted_data, data_size, data);

                    //similarly to previous, we mark a potential catch
                    possibly_caught = 1;
                }
            }
        break;
    }
    
    if (possibly_caught)
    {
        //WARNING: is_secret can compare unallocated data if received packet is too small
        secret_protocol = is_secret((char *)data);
        if (secret_protocol >= 0) //recognized a secret protocol
        {
            //now we know, that we have the real deal at hand and need to export all needed data into globals
            packet_was_caught = true;
            //the effect of all fabled corrupting
            recognized_protocol = corrupting ? SECRET_CORRUPTED : secret_protocol;
            /* following command is inefficient
             * inet_ntop already writes into buffer ip_src
             * so in case of ipv6 addresses the program writes twice into ip_src
             * this isn't a problem yet, just something to be aware of
             */
            strcpy(ip_src, ntohs(eptr->ether_type) == ETHERTYPE_IP ? inet_ntoa(my_ip->ip_src) : inet_ntop(AF_INET6, &my_ipv6->ip6_src, ip_src, INET6_ADDRSTRLEN));
            /* if the protocol was found to be corrupted there is no sense in trying to copy data
             * so the copying is skipped
             */
            if (secret_protocol != SECRET_CORRUPTED)
            {
                memcpy(decrypted_packet, (char *)data, data_size);
            }
            /* WARNING:
             * file doesn't export into any global how LONG the data are, this isn't a problem for how my protocol
             * is implemented, because short messages are ended by \n and longer ones have number how long they are
             * but if you are going to borrow this, beware that you might want to export length of packet as well
             */

            pcap_breakloop(handle);
        }
    }
}

/* There is a lot of commented code going on, it is because this function was taken from something else
 * I was slowly disabling contents of it, until I got into a state that was handy for me
 * I was reluctant to remove the code, in case I might need it, so it stayed
 */
/*==========================================================================
* Following Code taken from ISA examples
* (c) Petr Matousek, 2020
*///========================================================================
int listen_for_packet(){
  char errbuf[PCAP_ERRBUF_SIZE];  // constant defined in pcap.h
  pcap_if_t *alldev, *dev ;       // a list of all input devices
  char *devname;                  // a name of the device
  bpf_u_int32 netaddr;            // network address configured at the input device
  bpf_u_int32 mask;               // network mask of the input device
  struct bpf_program fp;          // the compiled filter

  //needed in an original exmaple, but not for me
  (void) dev;

  // open the input devices (interfaces) to sniff data
  if (pcap_findalldevs(&alldev, errbuf))
    err(1,"Can't open input device(s)");

  // list the available input devices
  #if 0
  printf("Available input devices are: ");
  for (dev = alldev; dev != NULL; dev = dev->next){
    printf("%s ",dev->name);
  }
  printf("\n");
  #endif

  if (isLoopback)
  {
      devname = "lo\0";
  }
  else
  {
    devname = alldev->name;  // select the name of first interface (default) for sniffing
  }

  // get IP address and mask of the sniffing interface
  if (pcap_lookupnet(devname,&netaddr,&mask,errbuf) == -1)
    err(1,"pcap_lookupnet() failed");

  #if 0
  struct in_addr a,b;
  a.s_addr=netaddr;
  //printf("Opening interface \"%s\" with net address %s,",devname,inet_ntoa(a));
  b.s_addr=mask;
  //printf("mask %s for listening...\n",inet_ntoa(b));
  #endif

  // open the interface for live sniffing
  if ((handle = pcap_open_live(devname,BUFSIZ,1,1000,errbuf)) == NULL)
    err(1,"pcap_open_live() failed");

  // compile the filter
  if (pcap_compile(handle,&fp,"icmp or icmp6",0,netaddr) == -1)
    err(1,"pcap_compile() failed");
  
  // set the filter to the packet capture handle
  if (pcap_setfilter(handle,&fp) == -1)
    err(1,"pcap_setfilter() failed");

  signal(SIGALRM, alarm_handler);
  alarm(TIMEOUT);

  // read packets from the interface in the infinite loop (count == -1)
  // incoming packets are processed by function mypcap_handler() 
  if (pcap_loop(handle,-1,mypcap_handler,NULL) == -1)
    err(1,"pcap_loop() failed");

  // close the capture device and deallocate resources
  pcap_close(handle);
  pcap_freealldevs(alldev);
  return 0;
}