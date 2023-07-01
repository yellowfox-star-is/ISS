#author: Václav Sysel
#VUT FIT, 5. semestr
#ISA, projekt
#varianta: skrytý kanál
#
#feel free to use, but if you are a student of VUT FIT, please no plagiarism *wink wink*
#my style of coding is fairly specific and I have no doubt, that you could be found out for it
#so... let be inspired but no steal steal <3


CFLAGS = -pedantic -Wall -Wextra

all: secret

clean:
	rm -f *.o secret

rebuild: all

debug: CFLAGS += -g
debug: all

archive: tar
tar:
	tar -cvf xsysel09.tar *.c *.h secret.1 manual.pdf Makefile

secret: secret.o error.o error.h bit.h arguments.o arguments.h networking.o networking.h client.h client.o smrcka_bat.o server.h server.o
	gcc $(CFLAGS) secret.o error.o arguments.o networking.o smrcka_bat.o client.o server.o -o secret -lcrypto -lpcap

secret.o: secret.c error.h bit.h networking.h arguments.h
	gcc $(CFLAGS) -c secret.c -o secret.o

client.o: client.h client.c smrcka_bat.h error.h networking.h
	gcc $(CFLAGS) -c client.c -o client.o

server.o: server.c server.h networking.h smrcka_bat.h error.h
	gcc $(CFLAGS) -c server.c -o server.o

networking.o: networking.h networking.c smrcka_bat.h error.h
	gcc $(CFLAGS) -c networking.c -o networking.o

arguments.o: arguments.h arguments.c networking.h error.h bit.h smrcka_bat.h
	gcc $(CFLAGS) -c arguments.c -o arguments.o

smrcka_bat.o: smrcka_bat.c smrcka_bat.h
	gcc $(CFLAGS) -c smrcka_bat.c -o smrcka_bat.o

error.o: error.h error.c
	gcc $(CFLAGS) -c error.c -o error.o

