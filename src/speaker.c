#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "speaker.h"

byte * wiiuse_convert_wav(const char *path, byte type) {

}

void wiiuse_mute_speaker(struct wiimote_t *wm, int status) {
	byte buf;

	if(!wm) {
		WIIUSE_ERROR("struct wiimote_t not inizialized");
		return;
	}
	if(status) {
		buf = 0x04;
		if(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER) && !WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER_MUTE)) {
			WIIUSE_DEBUG("muting speaker...");
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_MUTE);
			wiiuse_send(wm, WM_CMD_SPEAKER_MUTE, &buf, 1);
			WIIUSE_DEBUG("muted.");
		}
		else
			WIIUSE_INFO("speaker already muted or not initialized.");
	}
	else {
		buf = 0x00;
		if(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER) && WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER_MUTE)) {
			WIIUSE_DEBUG("unmuting speaker...");
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_MUTE);
			wiiuse_send(wm, WM_CMD_SPEAKER_MUTE, &buf, 1);
			WIIUSE_DEBUG("unmuted.");
		}
		else
			WIIUSE_INFO("speaker already unmuted or not initialized.");	
	}
}

void wiiuse_set_speaker(struct wiimote_t *wm,int status)
{
	if(!wm) return;
	
	if(status) {
		if(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER))
			WIIUSE_INFO("speaker already initialized.");
		else {
			byte conf[7];
			byte buf = 0x04;
			
			conf[0] = 0x00;
			conf[1] = 0x00;
			conf[2] = WM_FREQ_3000_LOWEST;
			conf[3] = WM_FREQ_3000_HIGHEST;
			conf[4] = DEFAULT_VOLUME;
			conf[5] = 0x00;
			conf[6] = 0x00;
			
			WIIUSE_DEBUG("initializing speaker...");
			wiiuse_send(wm, WM_CMD_SPEAKER_ENABLE, &buf, 1);
			wiiuse_send(wm, WM_CMD_SPEAKER_MUTE, &buf, 1);
			buf = 0x01;
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG3, &buf, 1);
			buf = 0x08;
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG1, &buf, 1);
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG1, conf, 7);
			buf = 0x01;
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG2, &buf, 1);
			buf = 0x00;
			wiiuse_send(wm, WM_CMD_SPEAKER_MUTE, &buf, 1);
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_SPEAKER);
			WIIUSE_DEBUG("speaker initialized.");
		}
	}
	else {
		if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER)) {
			WIIUSE_INFO("speaker not initialized.");
		}
		else {
			byte buf = 0x04;
			WIIUSE_DEBUG("freeing speaker...");
			wiiuse_send(wm, WM_CMD_SPEAKER_MUTE, &buf, 1);
			buf = 0x00;
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG3, &buf, 1);
			wiiuse_write_data(wm, WM_REG_SPEAKER_REG1, &buf, 1);
			wiiuse_send(wm, WM_CMD_SPEAKER_ENABLE, &buf, 1);
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_SPEAKER);
			WIIUSE_DEBUG("speaker freed.");
		}
	}
}

void wiiuse_enable_speaker(struct wiimote_t *wm) {
	wiiuse_set_speaker(wm, 1);
}

void wiiuse_disable_speaker(struct wiimote_t *wm) {
	wiiuse_set_speaker(wm, 0);
}

void wiiuse_set_speaker_freq(struct wiimote_t *wm, int freq) {
	byte buf;

	if(!wm) return;
	if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER)) {
		WIIUSE_INFO("speaker not initialized.");
		return;
	}
	
	WIIUSE_DEBUG("writing new freq...");
	switch(freq) {
		case 3000:
			buf = WM_FREQ_3000_LOWEST;
			wiiuse_write_data(wm, WM_REG_SPEAKER_FREQ_LOWEST, &buf, 1);
			buf = WM_FREQ_3000_HIGHEST;
			wiiuse_write_data(wm, WM_REG_SPEAKER_FREQ_HIGHEST, &buf, 1);
			break;
	}
	WIIUSE_DEBUG("written.");
}

void wiiuse_set_speaker_vol(struct wiimote_t *wm, byte vol) {
	if(!wm) return;
	if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER)) {
		WIIUSE_INFO("speaker not initialized.");
		return;
	}
	
	WIIUSE_DEBUG("writing new vol...");
	wiiuse_write_data(wm, WM_REG_SPEAKER_VOL, &vol, 1);
	WIIUSE_DEBUG("written.");
}

void wiiuse_play_sound(struct wiimote_t* wm, byte* data, int size) {
	byte report[21];
	int offset = 0;
	int to_be_written, i;

	if(!wm) return;
	if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_SPEAKER)) {
		WIIUSE_INFO("speaker not initialized.");
		return;
	}
	
	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_PLAYING);	
	while(offset < size) {
		if(size - offset > 20)
			to_be_written = 20;
		else
			to_be_written = size - offset;
		
		report[0] = to_be_written << 3;
		
		for(i = 1; i < 21; i++) {
			if(i <= to_be_written)
				report[i] = data[i - 1 + offset];
			else
				report[i] = 0x00;
		}
		
		wiiuse_send(wm, WM_CMD_SPEAKER_DATA, report, 21);
		
		offset += to_be_written;
	}
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_PLAYING);
}

