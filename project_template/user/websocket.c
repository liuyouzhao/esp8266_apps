/********************************************************************************
 * core/rc_task.c
 *
 *   Copyright (C) 2016,2017 CaiNiao(Clumsy Bird). All rights reserved.
 *   Author: Frodo Liu <liuyouzhao@hotmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name RawCode nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************/

#include <nopoll/nopoll.h>
#include "ssl_compat-1.0.h"
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "user_utils.h"

nopoll_bool debug = nopoll_false;

typedef struct ws_s
{
    noPollCtx  *ctx;
	noPollConn *conn;
    int (*connect) (const char *, const char *, const char *);
    int (*disconnect) ();
    int (*send) (const char *, int);
    int (*recv) (const char *);
} ws_t;

PREDEF_PTR(ws_t, ws)

static void __report_critical (noPollCtx * ctx, noPollDebugLevel level, const char * log_msg, noPollPtr user_data)
{
    if (level == NOPOLL_LEVEL_CRITICAL) {
        printf ("CRITICAL: %s\n", log_msg);
	}
	return;
}

static noPollCtx * create_ctx (void) {
	
	/* create a context */
	noPollCtx * ctx = nopoll_ctx_new ();
	nopoll_log_enable (ctx, debug);
	nopoll_log_color_enable (ctx, debug);
	return ctx;
}

static int ws_connect(const char *host, const char *port, const char *url)
{
    noPollCtx *ctx = create_ctx();
    noPollConn *conn = NULL;

    /* check connections registered */
	if (nopoll_ctx_conns (ctx) != 0) {
		printf ("ERROR: expected to find 0 registered connections but found: %d\n", nopoll_ctx_conns (ctx));
		return nopoll_false;
	} /* end if */

    nopoll_ctx_unref (ctx);

	/* reinit again */
	ctx = create_ctx ();

    /* call to create a connection */
	conn = nopoll_conn_new (ctx, host, port, NULL, url, NULL, NULL);
	if (! nopoll_conn_is_ok (conn)) {
		printf ("ERROR:error.. (conn=%p, conn->session=%d, NOPOLL_INVALID_SOCKET=%d, errno=%d, strerr=%s)..\n",
			conn, (int) nopoll_conn_socket (conn), (int) NOPOLL_INVALID_SOCKET, errno, strerror (errno));
		return nopoll_false;
	}

    /* bind env. */
    IPTR(ws)->ctx = ctx;
    IPTR(ws)->conn = conn;
}

static int ws_disconnect()
{
    /* finish connection */
	nopoll_conn_close (IPTR(ws)->conn);
	
	/* finish */
	nopoll_ctx_unref (IPTR(ws)->ctx);
}

static int ws_send(const char *msg, int len)
{
    /* send content text(utf-8) */
	if (nopoll_conn_send_text (IPTR(ws)->conn, msg, len) != len) {
		printf ("ERROR: Expected to find proper send operation..\n");
		return nopoll_false;
	}
}

static int ws_recv(char *msg)
{
    char *message = NULL;
    noPollMsg  * pmsg;
    if ((pmsg = nopoll_conn_get_msg (IPTR(ws)->conn)) == NULL) {

		if (! nopoll_conn_is_ok (IPTR(ws)->conn)) {
			printf ("ERROR: received websocket connection close during wait reply..\n");
			return nopoll_false;
		}
        printf("Received NULL message, it is unprecedented");
        return 0;
	}
    message = (char *) nopoll_msg_get_payload (pmsg);
    memcpy(msg, message, strlen(message));
    
    return strlen(msg);
}

/* websocket in static region */
static ws_t s_ptr_ws = {
    .ctx = NULL,
    .conn = NULL,    
    .connect = ws_connect,
    .disconnect = ws_disconnect,
    .send = ws_send,
    
};

/* Export global pointer */
EXPORT_PTR_V(ws_t, ws, &s_ptr_ws)

