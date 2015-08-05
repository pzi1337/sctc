/** \file audio.h
 *  \brief Header containing the types of the functions potentially exported by AO modules.
 */

#ifndef _AUDIO_H
	#define _AUDIO_H

	//\cond
	#include <stdbool.h>
	#include <stddef.h>
	#include <sys/types.h>
	//\endcond

	/** \brief The type of the function used to update the underlying sound system's format.
	 *
	 *  The corresponding function is expected to be exported as *audio_set_format* in the final module.
	 *
	 *  \remark This function is *required* to allow playback of audio.
	 *          AO modules not exporting this functions are invalid and therefore will not be loaded.
	 *
	 *  \param encoding  The encoding of the data passed to *audio_play* in the subsequent calls
	 *  \param rate      The rate of the data passed to *audio_play*
	 *  \param channels  The number of channels
	 *  \returns         `true` on success, `false` otherwise
	 */
	typedef bool (*audio_set_format_t)(unsigned int encoding, unsigned int rate, unsigned int channels);

	/** \brief The type of the function used to update volume.
	 *
	 *  The corresponding function is expected to be exported as *audio_change_volume* in the final module.
	 *
	 *  \remark This function is *optional*.
	 *
	 *  \param delta  The relative offset in percent (+10 -> increase volume by 10 percent)
	 *  \returns      The absolute volume (in percent) after changing.
	 */
	typedef int  (*audio_change_volume_t)(off_t delta);

	/** \brief The type of the function used to get the current volume.
	 *
	 *  The corresponding function is expected to be exported as *audio_get_volume* in the final module.
	 *
	 *  \remark This function is *optional*.
	 *
	 *  \returns      The absolute volume (in percent).
	 */
	typedef unsigned int (*audio_get_volume_t)(void);

	/** \brief The type of the function used to send raw audio data to the unterlying sound system.
	 *
	 *  The corresponding function is expected to be exported as *audio_play* in the final module.
	 *
	 *  \remark This function is *required* to allow playback of audio.
	 *          AO modules not exporting this functions are invalid and therefore will not be loaded.
	 *
	 *  \param buffer  The buffer containing the data.
	 *  \param size    The number of bytes ready in `buffer`.
	 *  \returns       The number of bytes played (may be less than `size`)
	 */
	typedef size_t (*audio_play_t)(void *buffer, size_t size);

	/** \brief The type of the function used to initialize the AO module.
	 *
	 *  Tries to initialize the underlying soundsystem.
	 *  If the underlying soundsystem is not available at all, it returns `false`, such that
	 *  the `audio_init` function can be used for probing.
	 *  The corresponding function is expected to be exported as *audio_init* in the final module.
	 *
	 *  \remark This function is *required* to allow playback of audio.
	 *          AO modules not exporting this functions are invalid and therefore will not be loaded.
	 *
	 *  \return `true` if the module was loaded successfully, `false` otherwise.
	 */
	typedef bool (*audio_init_t)(void);

	bool ao_module_load(char *lib, audio_init_t *audio_init, audio_play_t *audio_play, audio_set_format_t *audio_set_format, audio_get_volume_t *audio_get_volume, audio_change_volume_t *audio_change_volume);

	void ao_module_unload(void);
#endif
