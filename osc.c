/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */


/*
 *  Copyright (C) 2004 Steve Harris, Uwe Koloska
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#define _GNU_SOURCE /* strdupa() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "osc.h"
#include "player.h"
#include "deck.h"
#include "track.h"
#include "cues.h"
#include "player.h"

#include "lo/lo.h"

lo_server_thread st;
lo_server_thread st_tcp;
pthread_t thread_osc_updater;
struct deck *osc_deck;
struct library *osc_library;
int osc_ndeck = 0;
lo_address address[2];
int osc_nconnection = 0;
int osc_nclient = 0;


/*
 * Start osc server and add methods
 */

int osc_start(struct deck *deck, struct library *library, size_t ndeck)
{
    osc_deck = deck;
    osc_library = library;
    osc_ndeck = ndeck;

    /* why do we have two addresses here? They're both the same! */
    address[0] = lo_address_new_from_url("osc.udp://0.0.0.0:7771/");
    address[1] = lo_address_new_from_url("osc.udp://0.0.0.0:7771/");

    /* start a new server on port 7770 */
    st = lo_server_thread_new("7770", error);

    lo_server_thread_add_method(st, "/xwax/load_track", "isss", load_track_handler, NULL);
	lo_server_thread_add_method(st, "/xwax/cycle_timecoder", "", cycle_timecoder_handler, NULL);

    lo_server_thread_start(st);

    return 0;
}

/*
 * Stop all osc services
 */
void osc_stop()
{
    lo_server_thread_free(st);
}

/*
 * print error message about osc related things
 */

void error(int num, const char *msg, const char *path)
{
    fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stderr);
}

/*
 * catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods
 */

int generic_handler(const char *path, const char *types, lo_arg ** argv,
                    int argc, void *data, void *user_data)
{
    int i;

    fprintf(stderr, "Unsupported OSC message. path: <%s>\n", path);
    for (i = 0; i < argc; i++) {
        fprintf(stderr, "arg %d '%c' ", i, types[i]);
        lo_arg_pp((lo_type)types[i], argv[i]);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    fflush(stderr);

    return 1;
}

/*
 * callback function for /xwax/load_trackk
 */

int load_track_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    /* example showing pulling the argument values out of the argv array */
    fprintf(stderr, "%s <- deck:%i path:%s artist:%s title:%s\n",
            path, argv[0]->i, &argv[1]->s, &argv[2]->s, &argv[3]->s);
    fflush(stderr);

    int d;
    struct deck *de;
    struct record *r;

    d = argv[0]->i;
    if (d >= osc_ndeck) {
        fprintf(stderr, "Trying to access deck %d\n  osc_ndeck = %d\n", d, osc_ndeck);
        error(255, path, "Trying to access into invalid deck");
        return 255;
    }
    de = &osc_deck[d];

    r = malloc(sizeof *r);
    if (r == NULL) {
        perror("malloc");
        return -1;
    }

    r->pathname = strdup(&argv[1]->s);
    r->artist = strdup(&argv[2]->s);
    r->title = strdup(&argv[3]->s);  

    r = library_add(osc_library, r);
    if (r == NULL) {
        /* FIXME: memory leak, need to do record_clear(r) */
        return -1;
    }

    deck_load(&osc_deck[d], r);


    return 0;
}

int cycle_timecoder_handler()
{
struct timecoder *tc;
  struct deck  *de = &osc_deck[0];
	tc = &de->timecoder;
	
	timecoder_cycle_definition(tc);
}
