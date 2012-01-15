/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header$
 *
 */

/**
 *	@file
 *	@brief Handles device I/O (non-OS specific).
 */
#include "io.h"
#include "ir.h"                         /* for wiiuse_set_ir_mode */
#include "wiiuse_internal.h"

#include <stdlib.h>                     /* for free, malloc */

extern void propagate_event(struct wiimote_t* wm, byte event, byte* msg);

static void wiiuse_disable_motion_plus2(struct wiimote_t *wm,byte *data,unsigned short len)
{
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	wiiuse_set_ir_mode(wm);

	wm->handshake_state++;
	wiiuse_handshake(wm, NULL, 0);

}

static void wiiuse_disable_motion_plus1(struct wiimote_t *wm,byte *data,unsigned short len)
{
	byte val = 0x00;
	wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_disable_motion_plus2);
}

 /**
*    @brief Wait until specified report arrives and return it
*
*    @param wm        Pointer to a wiimote_t structure.
*    @param data      Pre-allocated memory to store the received data
*
*    Synchronous/blocking, this function will not return until it receives the specified
*    report from the Wiimote.
*
*/
void wiiuse_wait_report(struct wiimote_t *wm, int report, byte *data)
{
    for(;;)
    {
        if(wiiuse_io_read(wm, data, 32) > 0)
            if(data[1] == report)
                break;
    }
}

/**
*    @brief Read memory/register data synchronously
*
*    @param wm        Pointer to a wiimote_t structure.
*    @param memory    If set to non-zero, reads EEPROM, otherwise registers
*    @param addr      Address offset to read from
*    @param size      How many bytes to read
*    @param data      Pre-allocated memory to store the received data
*
*    Synchronous/blocking read, this function will not return until it receives the specified
*    amount of data from the Wiimote.
*
*/
void wiiuse_read(struct wiimote_t *wm, byte memory, unsigned addr, unsigned short size, byte *data)
{
    byte pkt[8];
    byte buf[32];
    unsigned n_full_reports;
    unsigned last_report;
    byte *output;
    int i;

    /*
     * address in big endian first, the leading byte will
     * be overwritten (only 3 bytes are sent)
     */
    to_big_endian_uint32_t(pkt + 2, addr);

    pkt[0] = 0x52; /* HID read command */
    pkt[1] = 0x17; /* report 17 - read */
    pkt[2] = (memory != 0) ? 0x00 : 0x04; /* read from registers or memory*/

    /*
     * length in big endian
     */
    to_big_endian_uint16_t(pkt + 6, size);

    /* calculate how many 16B packets we have to get back */
    n_full_reports = size / 16;
    last_report = size % 16;
    output = data;

    wiiuse_io_write(wm, pkt, 8);

    for(i = 0; i < n_full_reports; ++i)
    {
        wiiuse_wait_report(wm, 0x21, buf);
        memmove(output, buf + 7, 16);
        output += 16;
    }

    /* read the last incomplete packet */
    if(last_report)
    {
        wiiuse_wait_report(wm, 0x21, buf);
        memmove(output, buf + 7, last_report);
    }
}

 /**
 *	@brief Get initialization data from the wiimote.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param data		unused
 *	@param len		unused
 *
 *	When first called for a wiimote_t structure, a request
 *	is sent to the wiimote for initialization information.
 *	This includes factory set accelerometer data.
 *	The handshake will be concluded when the wiimote responds
 *	with this data.
 */

#define SYNC_HANDSHAKE 1
#ifdef SYNC_HANDSHAKE

void wiiuse_handshake(struct wiimote_t* wm, byte* data, uint16_t len)
{
    /* send request to wiimote for accelerometer calibration */
    byte buf[32];
    byte report_type;

    /* step 0 - Reset wiimote */
    {
        //wiiuse_set_leds(wm, WIIMOTE_LED_NONE);

        WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
        WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
        WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_ACC);
        WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
        WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
        WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
        WIIMOTE_DISABLE_FLAG(wm, WIIUSE_CONTINUOUS);

        wiiuse_set_report_type(wm);
        wiiuse_millisleep(500);

        WIIUSE_DEBUG("Wiimote reset!\n");
    }

    /* step 1 - calibration of accelerometers */
    {
        struct accel_t* accel = &wm->accel_calib;
        byte val;

        wiiuse_read(wm, 1, WM_MEM_OFFSET_CALIBRATION, 8, buf);

        /* received read data */
        accel->cal_zero.x = buf[0];
        accel->cal_zero.y = buf[1];
        accel->cal_zero.z = buf[2];

        accel->cal_g.x = buf[4] - accel->cal_zero.x;
        accel->cal_g.y = buf[5] - accel->cal_zero.y;
        accel->cal_g.z = buf[6] - accel->cal_zero.z;

        WIIUSE_DEBUG("Calibrated wiimote acc\n");
    }

    /* step 2 - re-enable IR and ask for status */
    {
        WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
        WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);

        /* now enable IR if it was set before the handshake completed */
         if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR))
         {
             WIIUSE_DEBUG("Handshake finished, enabling IR.");
             WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
             wiiuse_set_ir(wm, 1);
         }

         WIIUSE_DEBUG("Asking for status ...\n");
         wm->event = WIIUSE_CONNECT;
         wiiuse_status(wm);
    }
}

#else
void wiiuse_handshake(struct wiimote_t* wm, byte* data, uint16_t len) {
	if (!wm)	return;

	switch (wm->handshake_state) {
		case 0:
		{
			byte* buf;

            /* continous reporting off, report to buttons only */
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			wiiuse_set_leds(wm, WIIMOTE_LED_NONE);

            WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_ACC);
            WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
            WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
            WIIMOTE_DISABLE_FLAG(wm, WIIUSE_CONTINUOUS);

            wiiuse_set_report_type(wm);

            /* send request to wiimote for accelerometer calibration */
			buf = (byte*)malloc(sizeof(byte) * 8);
			wiiuse_read_data_cb(wm, wiiuse_handshake, buf, WM_MEM_OFFSET_CALIBRATION, 7);
			wm->handshake_state++;

			wiiuse_set_leds(wm, WIIMOTE_LED_NONE);

			break;
		}

		case 1:
		{
			struct read_req_t* req = wm->read_req;
			struct accel_t* accel = &wm->accel_calib;
			byte val;

			/* received read data */
			accel->cal_zero.x = req->buf[0];
			accel->cal_zero.y = req->buf[1];
			accel->cal_zero.z = req->buf[2];

			accel->cal_g.x = req->buf[4] - accel->cal_zero.x;
			accel->cal_g.y = req->buf[5] - accel->cal_zero.y;
			accel->cal_g.z = req->buf[6] - accel->cal_zero.z;

			/* done with the buffer */
			free(req->buf);

			/* handshake is done */
			WIIUSE_DEBUG("Handshake finished. Calibration: Idle: X=%x Y=%x Z=%x\t+1g: X=%x Y=%x Z=%x",
					accel->cal_zero.x, accel->cal_zero.y, accel->cal_zero.z,
					accel->cal_g.x, accel->cal_g.y, accel->cal_g.z);

			/* M+ off */
			val = 0x55;
			wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_disable_motion_plus1);

			break;
		}

		case 2:
		{
			/* request the status of the wiimote to check for any expansion */
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
			wm->handshake_state++;

			/* now enable IR if it was set before the handshake completed */
			if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR)) {
				WIIUSE_DEBUG("Handshake finished, enabling IR.");
				WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
				wiiuse_set_ir(wm, 1);
			}

			wm->event = WIIUSE_CONNECT;
			wiiuse_status(wm);

			break;
		}

		default:
		{
			break;
		}
	}
}
#endif
