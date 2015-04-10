.PHONY: all clean

CCOPT=-O0 -g -fstack-protector-strong --param=ssp-buffer-size=4
LDOPT=-pie -z relro -z now

CC=gcc
CFLAGS=`pkg-config --cflags ao yajl ncursesw libconfuse libxml-2.0 libmpg123` -std=gnu11 -Wall -pedantic -fPIC $(CCOPT)
#-Wextra
LDFLAGS=`pkg-config --libs ao yajl ncursesw libconfuse libxml-2.0 libmpg123` -lpolarssl -lpthread $(LDOPT)

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: soundcloud.o cache.o sound.o main.o helper.o log.o config.o http.o m3u.o xspf.o track.o tui.o url.o json.o network/tls.o network/plain.o
	gcc $+ $(LDFLAGS) -o ../bin/sctc

clean:
	@rm -rf *.o network/*.o
