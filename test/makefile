.PHONY = all clean


CCOPT=-O0 -g -fstack-protector-strong --param=ssp-buffer-size=4
LDOPT=-pie -z relro -z now

CC=gcc
CFLAGS=`pkg-config --cflags ao yajl ncursesw libconfuse libmpg123` -std=gnu11 -Wall -pedantic -fPIC $(CCOPT)
#-Wextra
LDFLAGS=`pkg-config --libs ao yajl ncursesw libconfuse libmpg123` -lpolarssl -lpthread $(LDOPT)

_%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

all: _main.o _helper.o
	# do a clean build of SCTC
	make -C ../src/ clean all
	gcc $(LDFLAGS) ../src/cache.o  ../src/command.o  ../src/config.o  ../src/helper.o  ../src/http.o  ../src/jspf.o  ../src/log.o  ../src/soundcloud.o  ../src/sound.o  ../src/state.o  ../src/track.o  ../src/tui.o  ../src/url.o  ../src/yajl_helper.o ../src/network/*.o $^ -o run_tests

clean:
	@rm -rf *.o run_tests
