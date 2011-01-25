/**
 *	@file
 *	@brief Handles speaker functions.
 */

#ifndef SPEAKER_H_INCLUDED
#define SPEAKER_H_INCLUDED

#include "wiiuse_internal.h"

#define DEFAULT_VOLUME	0x40

#ifdef __cplusplus
extern "C" {
#endif

byte * wiiuse_convert_wav(const char *path, byte type);
void wiiuse_mute_speaker(struct wiimote_t *wm, int status);
void wiiuse_set_speaker(struct wiimote_t *wm,int status);
void wiiuse_enable_speaker(struct wiimote_t *wm);
void wiiuse_disable_speaker(struct wiimote_t *wm);
void wiiuse_set_speaker_freq(struct wiimote_t *wm, int freq);
void wiiuse_set_speaker_vol(struct wiimote_t *wm, byte vol);
void wiiuse_play_sound(struct wiimote_t* wm, byte* data, int size);

#ifdef __cplusplus
}
#endif

#endif // SPEAKER_H_INCLUDED


