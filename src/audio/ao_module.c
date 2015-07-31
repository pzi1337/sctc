#include "ao_module.h"

//\cond
#include <dlfcn.h>
#include <stdint.h>
//\endcond

#include "../log.h"

static void *dl_ao = NULL;

bool ao_module_load(char *lib, audio_init_t *audio_init, audio_play_t *audio_play, audio_set_format_t *audio_set_format, audio_get_volume_t *audio_get_volume, audio_change_volume_t *audio_change_volume) {
	dl_ao = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
	if(!dl_ao) {
		_log("Not using %s: %s", lib, dlerror());
		return false;
	}

	// ofc this is ugly, but things will not work otherwise
	*audio_init          = (audio_init_t)          (intptr_t) dlsym(dl_ao, "audio_init");
	*audio_play          = (audio_play_t)          (intptr_t) dlsym(dl_ao, "audio_play");
	*audio_get_volume    = (audio_get_volume_t)    (intptr_t) dlsym(dl_ao, "audio_get_volume");
	*audio_change_volume = (audio_change_volume_t) (intptr_t) dlsym(dl_ao, "audio_change_volume");
	*audio_set_format    = (audio_set_format_t)    (intptr_t) dlsym(dl_ao, "audio_set_format");

	if(!*audio_init || !*audio_play || !*audio_set_format) {
		*audio_init          = NULL;
		*audio_play          = NULL;
		*audio_get_volume    = NULL;
		*audio_change_volume = NULL;
		*audio_set_format    = NULL;
		return false;
	}

	return true;
}

void ao_module_unload() {
	dlclose(dl_ao);
}
