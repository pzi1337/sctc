.PHONY: all clean

CCOPT=-O0 -g -fstack-protector-strong --param=ssp-buffer-size=4
LDOPT=-pie -z relro -z now

CC=gcc
CFLAGS=`pkg-config --cflags ao yajl ncursesw libconfuse libmpg123` -std=gnu11 -Wall -fPIC -pedantic $(CCOPT)
#-Wextra
LDFLAGS=`pkg-config --libs yajl ncursesw libconfuse libmpg123` -lpolarssl -ldl -lpthread $(LDOPT)

%.o: %.c
	@echo "CC\t"$@
	@$(CC) $(CFLAGS) -c $< -o $@

%.so: %.o
	@echo "LD\t"$@
	@$(CC) $(LDFLAGS) `pkg-config --libs ao` -rdynamic -shared $< -o ../bin/$@

all: sctc audio/ao.so

sctc: soundcloud.o cache.o command.o downloader.o state.o sound.o main.o helper.o log.o config.o http.o jspf.o track.o tui.o url.o yajl_helper.o network/tls.o network/plain.o
	@echo "LD\t"$@
	@$(CC) $+ $(LDFLAGS) -rdynamic -o ../bin/sctc

clean:
	@rm -rf *.o network/*.o audio/*.o
