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
 *	@brief Handles device I/O for platforms supported by bt-embedded.
 */

#include "wiiuse_internal.h" /* for WM_RPT_CTRL_STATUS */
#include "events.h"
#include "io.h"
#include "os.h"

#ifdef WIIUSE_BT_EMBEDDED

#include <bt-embedded/bte.h>
#include <bt-embedded/client.h>
#include <bt-embedded/hci.h>
#include <bt-embedded/l2cap.h>
#include <bt-embedded/services/hid.h>

#include <stdbool.h>
#include <time.h>       /* for clock_gettime */

typedef struct {
    struct wiimote_t **wm;
    int max_wiimotes;
    int num_found;
    bool done;
} WiiuseBtSearch;

static char s_nintendo_rvl[] = "Nintendo RVL-CNT-01";
static BteClient *s_client = NULL;
static struct wiimote_t *s_sync_read_target = NULL;
static byte *s_sync_read_target_buf;
static int s_sync_read_target_len;

static void initialized_cb(BteHci *hci, bool success, void *userdata)
{
    (void)hci;
    (void)success;
    bool *done = userdata;
    *done = true;
}

static void wait_until_done(unsigned long *timeout_ms, bool *done)
{
    while (!*done && *timeout_ms > 0) {
        unsigned long before = wiiuse_os_ticks();
        bte_wait_events(*timeout_ms * 1000);
        unsigned long elapsed = wiiuse_os_ticks() - before;
        if (elapsed > *timeout_ms)
        {
            *timeout_ms = 0;
        }
        else
        {
            *timeout_ms -= elapsed;
        }
    }
}

static bool wiimote_known(const BteBdAddr *address,
                          struct wiimote_t **wiimotes, int num_wiimotes)
{
    for (int i = 0; i < num_wiimotes; i++) {
        const struct wiimote_t *wm = wiimotes[i];
        if (memcmp(&wm->address, address, sizeof(wm->address)) == 0) {
            return true;
        }
    }
    return false;
}

static void read_remote_name_cb(BteHci *hci, const BteHciReadRemoteNameReply *r,
                                void *userdata)
{
    (void)hci;
    WiiuseBtSearch *ctx = userdata;

	if (r->status != 0 ||
        strncmp(r->name, s_nintendo_rvl, sizeof(s_nintendo_rvl) - 1) != 0 ||
        wiimote_known(&r->address, ctx->wm, ctx->num_found)) return;

    struct wiimote_t *wm = ctx->wm[ctx->num_found++];
    wm->address = r->address;
    WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_DEV_FOUND);
    if (strcmp(r->name + sizeof(s_nintendo_rvl) - 1, "-TR") == 0)
        wm->type = WIIUSE_WIIMOTE_MOTION_PLUS_INSIDE;

    if (ctx->num_found >= ctx->max_wiimotes)
        ctx->done = true;
}

static void inquiry_cb(BteHci *hci, const BteHciInquiryReply *reply, void *userdata)
{
    WiiuseBtSearch *ctx = userdata;

	if (reply->status != 0) return;

	WIIUSE_DEBUG("Results: %d", reply->num_responses);

    if (ctx->num_found >= ctx->max_wiimotes) {
        ctx->done = true;
        return;
    }

	for (int i = 0; i < reply->num_responses; i++) {
		const BteHciInquiryResponse *r = &reply->responses[i];

        uint8_t major = bte_cod_get_major_dev_class(r->class_of_device);
        uint8_t minor = bte_cod_get_minor_dev_class(r->class_of_device);
        if (major != BTE_COD_MAJOR_DEV_CLASS_PERIPH || (minor != 1 && minor != 2))
            continue;

        /* If we already know that this is a wiimote, skip it */
        if (wiimote_known(&r->address, ctx->wm, ctx->num_found))
            continue;

        bte_hci_read_remote_name(hci, &r->address, r->page_scan_rep_mode,
                                 r->clock_offset, NULL, read_remote_name_cb, ctx);
        /* We cannot run more than one name query at once, so let's stop this
         * iteration here */
        break;
	}
}

static void deliver_queued_data(struct wiimote_t *wm)
{
    BteBuffer *next = NULL;
    for (BteBuffer *buffer = wm->incoming_queue; buffer != NULL; buffer = next)
    {
        uint8_t *data = buffer->data;
        data += 4 + 1; /* Skip ACL header size and Bluetooth HID data byte */

        propagate_event(wm, data[0], data + 1);
        next = buffer->next;
        bte_buffer_unref(buffer);
    }
}

static void message_received_cb(BteL2cap *l2cap, BteBufferReader *reader, void *userdata)
{
    (void)l2cap;
	struct wiimote_t *wm = userdata;
    uint16_t size = 0;
	uint8_t *data = bte_buffer_reader_read_max(reader, &size);
	if (!data || size < 2) return;

	uint8_t hdr = data[0];
	uint8_t type = hdr & BTE_HID_HDR_TRANS_MASK;
    if (type != BTE_HID_TRANS_DATA) return;

    data++;
    size--;
    WIIUSE_DEBUG("Got report %02x", data[0]);
    if (s_sync_read_target == wm)
    {
        WIIUSE_DEBUG("Synchronously delivering report");
        if (size > s_sync_read_target_len)
            size = s_sync_read_target_len;
        memcpy(s_sync_read_target_buf, data, size);
    }
    else if (s_sync_read_target != NULL)
    {
        WIIUSE_DEBUG("Queuing report");
        wm->incoming_queue = bte_buffer_append(wm->incoming_queue, reader->buffer);
    }
    else
    {
        propagate_event(wm, data[0], data + 1);
    }
}

static void disconnected_cb(BteL2cap *l2cap, uint8_t reason, void *userdata)
{
    (void)l2cap;
    struct wiimote_t *wm = userdata;

    if (wm->ctrl_channel)
    {
        bte_l2cap_unref(wm->ctrl_channel);
        wm->ctrl_channel = NULL;
    }

    if (wm->intr_channel)
    {
        bte_l2cap_unref(wm->intr_channel);
        wm->intr_channel = NULL;
    }

    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
    wm->event = reason == BTE_HCI_CONN_TERMINATED_BY_LOCAL_HOST ?
        WIIUSE_DISCONNECT : WIIUSE_UNEXPECTED_DISCONNECT;
    /* propagate the event:
       Emit a controller-status type event. */
    propagate_event(wm, WM_RPT_CTRL_STATUS, 0);
}

static void intr_channel_connected_cb(BteL2cap *l2cap, const BteL2capNewConfiguredReply *reply, void *userdata)
{
    struct wiimote_t *wm = userdata;

    WIIUSE_DEBUG("result %d", reply->result);
    if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK)
    {
        bte_l2cap_unref(wm->ctrl_channel);
        wm->ctrl_channel = NULL;
        wm->event = WIIUSE_UNEXPECTED_DISCONNECT;
        return;
    }

    wm->intr_channel = bte_l2cap_ref(l2cap);
	bte_l2cap_on_message_received(wm->ctrl_channel, message_received_cb);
	bte_l2cap_on_message_received(wm->intr_channel, message_received_cb);
    bte_l2cap_on_acl_disconnected(wm->ctrl_channel, disconnected_cb);
    WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
}

static void ctrl_channel_connected_cb(BteL2cap *l2cap, const BteL2capNewConfiguredReply *reply, void *userdata)
{
    struct wiimote_t *wm = userdata;

    WIIUSE_DEBUG("result %d", reply->result);
    if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK)
    {
        wm->event = WIIUSE_UNEXPECTED_DISCONNECT;
        return;
    }

    wm->ctrl_channel = bte_l2cap_ref(l2cap);
    bte_l2cap_new_configured(s_client, &wm->address, BTE_L2CAP_PSM_HID_INTR,
                             NULL, BTE_L2CAP_CONNECT_FLAG_NONE, NULL,
                             intr_channel_connected_cb, wm);
}

static int wiiuse_os_connect_single(struct wiimote_t *wm)
{
    if (!wm || WIIMOTE_IS_CONNECTED(wm))
    {
        return 0;
    }

    bte_l2cap_new_configured(s_client, &wm->address, BTE_L2CAP_PSM_HID_CTRL,
                             NULL, BTE_L2CAP_CONNECT_FLAG_NONE, NULL,
                             ctrl_channel_connected_cb, wm);

    while (!WIIMOTE_IS_CONNECTED(wm) && wm->event != WIIUSE_UNEXPECTED_DISCONNECT) {
        bte_wait_events(10000);
    }
    WIIUSE_INFO("Connected to wiimote [id %i].", wm->unid);

    if (!WIIMOTE_IS_CONNECTED(wm))
        return 0;

    /* do the handshake */
    wiiuse_handshake(wm, NULL, 0);

    wiiuse_set_report_type(wm);

    return 1;
}

int wiiuse_os_find(struct wiimote_t **wm, int max_wiimotes, int timeout)
{
    unsigned long timeout_ms = timeout * 1000;
    BteHci *hci;

    if (!s_client)
    {
        s_client = bte_client_new();
        hci = bte_hci_get(s_client);
        bool done = false;
        bte_hci_on_initialized(hci, initialized_cb, &done);
        wait_until_done(&timeout_ms, &done);
    }
    else
    {
        hci = bte_hci_get(s_client);
    }

    /* Start searching for devices */
    WiiuseBtSearch ctx;
    ctx.done = false;
    ctx.wm = wm;
    ctx.max_wiimotes = max_wiimotes;
    ctx.num_found = 0;
    bte_hci_periodic_inquiry(hci, 4, 5, BTE_LAP_LIAC, 3, 4, NULL, inquiry_cb, &ctx);
    wait_until_done(&timeout_ms, &ctx.done);

    bte_hci_exit_periodic_inquiry(hci, NULL, NULL);
    return ctx.num_found;
}

/**
 *	@see wiiuse_connect()
 */
int wiiuse_os_connect(struct wiimote_t **wm, int wiimotes)
{
    int connected = 0;
    int i         = 0;

    for (; i < wiimotes; ++i)
    {
        if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND))
        /* if the device address is not set, skip it */
        {
            continue;
        }

        if (wiiuse_os_connect_single(wm[i]))
        {
            ++connected;
        }
    }

    return connected;
}

void wiiuse_os_disconnect(struct wiimote_t *wm)
{
    if (!wm || WIIMOTE_IS_CONNECTED(wm))
    {
        return;
    }

    if (wm->intr_channel)
    {
        bte_l2cap_disconnect(wm->intr_channel);
        bte_l2cap_unref(wm->intr_channel);
        wm->intr_channel = NULL;
    }
    if (wm->ctrl_channel)
    {
        bte_l2cap_disconnect(wm->ctrl_channel);
        bte_l2cap_unref(wm->ctrl_channel);
        wm->ctrl_channel = NULL;
    }

    wm->event    = WIIUSE_NONE;

    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
}

int wiiuse_os_poll(struct wiimote_t **wm, int wiimotes)
{
    /* If we had queued incoming data, deliver it now */
    for (int i = 0; i < wiimotes; i++)
    {
        deliver_queued_data(wm[i]);
    }
    return bte_wait_events(500);
}

int wiiuse_os_read(struct wiimote_t *wm, byte *buf, int len)
{
    /* There's no efficient way to implement this function correctly: at the
     * Bluetooth controller level, there is no concept of file descriptor, but
     * rather the events are propagated as they are received, regardless of the
     * remote device they are coming from.
     * Implementing this function correctly requires putting all the incoming
     * messages into a per-wiimote queue, which consumes memory and introduces
     * latency on all other devices.
     * In the other platform backends this is not seen as a problem because the
     * OS abstractions already provide this, but for embedded devices this is a
     * pain.
     */
    s_sync_read_target = wm;
    s_sync_read_target_buf = buf;
    s_sync_read_target_len = len;
    int rc = bte_wait_events(10);
    s_sync_read_target = NULL;

/* log the received data */
#ifdef WITH_WIIUSE_DEBUG
    if (rc > 0)
    {
        if (buf[0] != 0x30)
        { /* hack for chatty Balance Boards that flood the logs with useless button reports */
            int i;
            printf("[DEBUG] (id %i) RECV: (%.2x) ", wm->unid, buf[0]);
            for (i = 1; i < rc - 1; i++)
            {
                printf("%.2x ", buf[i]);
            }
            printf("\n");
        }
    }
#endif

    return rc;
}

int wiiuse_os_write(struct wiimote_t *wm, byte report_type, byte *buf, int len)
{
    if (!wm->intr_channel) return -1;

    BteBufferWriter writer;
    bte_l2cap_create_message(wm->intr_channel, &writer, len + 2);
    uint8_t hid_hdr_and_report_type[2] = {
        BTE_HID_TRANS_DATA | BTE_HID_REP_TYPE_OUTPUT,
        report_type,
    };
    bte_buffer_writer_write(&writer, hid_hdr_and_report_type, 2);
    bte_buffer_writer_write(&writer, buf, len);
    return bte_l2cap_send_message(wm->intr_channel,
                                  bte_buffer_writer_end(&writer));
}

void wiiuse_init_platform_fields(struct wiimote_t *wm)
{
    wm->intr_channel = NULL;
}

void wiiuse_cleanup_platform_fields(struct wiimote_t *wm)
{
    (void)wm;
}

unsigned long wiiuse_os_ticks()
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    unsigned long ms = 1000 * tp.tv_sec + tp.tv_nsec / 1000000;
    return ms;
}

#endif /* ifdef WIIUSE_BT_EMBEDDED */
