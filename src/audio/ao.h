#ifndef _SOUND_AO_H
	#define _SOUND_AO_H

	#include <stdbool.h>
	#include <stddef.h>

	bool sound_ao_set_format(unsigned int bits, unsigned int rate, unsigned int channels);
	void sound_ao_play(void *buffer, size_t size);
	bool sound_ao_init();
#endif
