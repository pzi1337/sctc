.PHONY: all clean cppcheck

CCOPT=-O3 -s -fstack-protector-strong --param=ssp-buffer-size=4
LDOPT=-O3 -s -pie -z relro -z now

CCWARN=-Wall -Wextra -Wpedantic \
	-Wformat -Winit-self \
	-Wswitch-enum -Wswitch-default \
	-Winit-self \
	-Wcast-qual -Wcast-align \
	-Wold-style-definition \
	-Wstrict-prototypes \
	-Wshadow -Wundef -Winline \
	-Wnonnull \
	-Wmissing-field-initializers \
	-Wlogical-op -Wfloat-equal \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wnested-externs \
	-fno-omit-frame-pointer -ffloat-store  -fstrict-aliasing \
	-fno-common
#-Wbad-function-cast

CC=gcc
CFLAGS=-D_GNU_SOURCE `pkg-config --cflags yajl ncursesw libconfuse libmpg123` -std=gnu11 $(CCWARN) -fPIC -fdiagnostics-color=auto $(CCOPT)
LDFLAGS=`pkg-config --libs yajl ncursesw libconfuse libmpg123` -lpolarssl -ldl -lpthread $(LDOPT)

CFILES_MAIN=soundcloud.c cache.c command.c downloader.c state.c sound.c main.c helper.c log.c config.c http.c jspf.c track.c tui.c url.c yajl_helper.c audio/ao_module.c network/tls.c network/plain.c
OFILES_MAIN=$(CFILES_MAIN:.c=.o)
CFILES_AO=audio/ao.c
OFILES_AO=$(CFILES_AO:.c=.o)
CFILES_ALSA=audio/alsa.c
OFILES_ALSA=$(CFILES_ALSA:.c=.o)

%.o: %.c
	@echo "CC\t"$@
	@$(CC) $(CFLAGS) -c $< -o $@

all: sctc audio/alsa.so audio/ao.so

#############################
# anything cppcheck related #
#############################
CPPCHECK=cppcheck
CPPCHECK_FLAGS=-q --inconclusive --enable=all

cppcheck: $(CFILES_MAIN) $(CFILES_AO) $(CFILES_ALSA)
	@$(CPPCHECK) $(CPPCHECK_FLAGS) $^


##########################
# the ALSA output plugin #
##########################
LDFLAGS_ALSA_SO=`pkg-config --libs alsa` -rdynamic -shared
audio/alsa.so: $(OFILES_ALSA)
	@echo "LD\t"$@
	@$(CC) $(LDOPT) $(LDFLAGS_ALSA_SO) $< -o ../bin/$@


###################################
# the AO output plugin *fallback* #
###################################
LDFLAGS_AO_SO=`pkg-config --libs ao` -rdynamic -shared
audio/ao.so: $(OFILES_AO)
	@echo "LD\t"$@
	@$(CC) $(LDOPT) $(LDFLAGS_AO_SO) $< -o ../bin/$@


####################
# the main program #
####################
sctc: $(OFILES_MAIN)
	@echo "LD\t"$@
	@$(CC) $+ $(LDFLAGS) -rdynamic -o ../bin/sctc

clean:
	@rm -rf *.o network/*.o audio/*.o
