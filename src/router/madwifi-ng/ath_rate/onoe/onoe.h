/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: onoe.h 1441 2006-02-06 16:03:21Z mrenzmann $
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_ATH_RATE_ONOE_H
#define _DEV_ATH_RATE_ONOE_H

/* per-device state */
struct onoe_softc {
	struct ath_ratectrl arc;		/* base state */
};

/* per-node state */
struct onoe_node {
	u_int on_tx_ok;			/* tx ok pkt */
	u_int on_tx_err; 		/* tx !ok pkt */
	u_int on_tx_retr;		/* tx retry count */
	int on_tx_upper; 		/* tx upper rate req cnt */
	u_int8_t	on_tx_rix0; 		/* series 0 rate index */
	u_int8_t	on_tx_try0; 		/* series 0 try count */
	u_int8_t	on_tx_rate0;		/* series 0 h/w rate */
	u_int8_t	on_tx_rate1;		/* series 1 h/w rate */
	u_int8_t	on_tx_rate2;		/* series 2 h/w rate */
	u_int8_t	on_tx_rate3;		/* series 3 h/w rate */
	u_int8_t	on_tx_rate0sp;		/* series 0 short preamble h/w rate */
	u_int8_t	on_tx_rate1sp;		/* series 1 short preamble h/w rate */
	u_int8_t	on_tx_rate2sp;		/* series 2 short preamble h/w rate */
	u_int8_t	on_tx_rate3sp;		/* series 3 short preamble h/w rate */
	int on_nextcheck;		/* time of next check for rate drop */
};
#define	ATH_NODE_ONOE(an)	((struct onoe_node *)&an[1])
#endif /* _DEV_ATH_RATE_ONOE_H */
