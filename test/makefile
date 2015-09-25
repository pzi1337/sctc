.PHONY = all run clean


CCOPT=-O0 -g -fstack-protector-strong --param=ssp-buffer-size=4
LDOPT=-O0 -g -pie -z relro -z now

CC=gcc
CFLAGS=`pkg-config --cflags ao yajl ncursesw libconfuse libmpg123` -std=gnu11 -Wall -pedantic -fPIC $(CCOPT)
#-Wextra
LDFLAGS=`pkg-config --libs ao yajl ncursesw libconfuse libmpg123` -lpolarssl -lpthread -ldl $(LDOPT)

_%.o: %.c
	@echo "CC\t"$@
	@gcc $(CFLAGS) -c $< -o $@

all: _main.o _helper.o _plain.o _url.o _tls.o _http.o additions/file.o
	@echo ""
	@echo Building SCTC
	@make -C ../src/ clean all
	@echo "LD\trun_tests"
	@gcc $(LDFLAGS) \
		../src/cache.o ../src/command.o ../src/config.o ../src/downloader.o ../src/helper.o ../src/http.o ../src/jspf.o ../src/log.o ../src/sound.o ../src/soundcloud.o ../src/state.o ../src/track.o ../src/tui.o ../src/url.o ../src/yajl_helper.o \
		../src/network/*.o ../src/commands/*.o ../src/audio/ao_module.o $^ -o run_tests

run: all
	valgrind --leak-check=full --show-leak-kinds=all -v --log-file=run_tests_valgrind.log ./run_tests

clean:
	@rm -rf *.o run_tests
