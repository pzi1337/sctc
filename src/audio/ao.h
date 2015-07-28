#ifndef _AUDIO_H
	#define _AUDIO_H

	#include <stdbool.h>
	#include <stddef.h>

	typedef bool (*audio_set_format_t)(unsigned int bits, unsigned int rate, unsigned int channels);
	typedef void (*audio_play_t)(void *buffer, size_t size);
	typedef bool (*audio_init_t)();
#endif
