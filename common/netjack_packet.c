
/*
 * NetJack - Packet Handling functions
 *
 * used by the driver and the jacknet_client
 *
 * Copyright (C) 2008 Marc-Olivier Barre <marco@marcochapeau.org>
 * Copyright (C) 2008 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 * Copyright (C) 2006 Torben Hohn <torbenh@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: net_driver.c,v 1.16 2006/03/20 19:41:37 torbenh Exp $
 *
 */

//#include "config.h"

#define HAVE_CELT 1

#define _XOPEN_SOURCE 600
#define _BSD_SOURCE

#if HAVE_PPOLL
#define _GNU_SOURCE
#endif

#include <math.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#include <jack/types.h>
//#include <jack/engine.h>

#include <sys/types.h>

#ifdef WIN32
#include <winsock2.h>
#include <malloc.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#endif

#include <errno.h>
#include <signal.h>

#if HAVE_SAMPLERATE
#include <samplerate.h>
#endif

#if HAVE_CELT
#include <celt/celt.h>
#endif

#include "netjack_packet.h"

// JACK2 specific.
#include "jack/control.h"

int fraggo = 0;

packet_cache *global_packcache = NULL;

void
packet_header_hton (jacknet_packet_header *pkthdr)
{
    pkthdr->capture_channels_audio = htonl(pkthdr->capture_channels_audio);
    pkthdr->playback_channels_audio = htonl(pkthdr->playback_channels_audio);
    pkthdr->capture_channels_midi = htonl(pkthdr->capture_channels_midi);
    pkthdr->playback_channels_midi = htonl(pkthdr->playback_channels_midi);
    pkthdr->period_size = htonl(pkthdr->period_size);
    pkthdr->sample_rate = htonl(pkthdr->sample_rate);
    pkthdr->sync_state = htonl(pkthdr->sync_state);
    pkthdr->transport_frame = htonl(pkthdr->transport_frame);
    pkthdr->transport_state = htonl(pkthdr->transport_state);
    pkthdr->framecnt = htonl(pkthdr->framecnt);
    pkthdr->latency = htonl(pkthdr->latency);
    pkthdr->reply_port = htonl(pkthdr->reply_port);
    pkthdr->mtu = htonl(pkthdr->mtu);
    pkthdr->fragment_nr = htonl(pkthdr->fragment_nr);
}

void
packet_header_ntoh (jacknet_packet_header *pkthdr)
{
    pkthdr->capture_channels_audio = ntohl(pkthdr->capture_channels_audio);
    pkthdr->playback_channels_audio = ntohl(pkthdr->playback_channels_audio);
    pkthdr->capture_channels_midi = ntohl(pkthdr->capture_channels_midi);
    pkthdr->playback_channels_midi = ntohl(pkthdr->playback_channels_midi);
    pkthdr->period_size = ntohl(pkthdr->period_size);
    pkthdr->sample_rate = ntohl(pkthdr->sample_rate);
    pkthdr->sync_state = ntohl(pkthdr->sync_state);
    pkthdr->transport_frame = ntohl(pkthdr->transport_frame);
    pkthdr->transport_state = ntohl(pkthdr->transport_state);
    pkthdr->framecnt = ntohl(pkthdr->framecnt);
    pkthdr->latency = ntohl(pkthdr->latency);
    pkthdr->reply_port = ntohl(pkthdr->reply_port);
    pkthdr->mtu = ntohl(pkthdr->mtu);
    pkthdr->fragment_nr = ntohl(pkthdr->fragment_nr);
}

int get_sample_size (int bitdepth)
{
    if (bitdepth == 8)
        return sizeof (int8_t);
    if (bitdepth == 16)
        return sizeof (int16_t);
    if( bitdepth == 1000 )
	return sizeof( unsigned char );
    return sizeof (int32_t);
}

// fragment management functions.

packet_cache
*packet_cache_new (int num_packets, int pkt_size, int mtu)
{
    int fragment_payload_size = mtu - sizeof (jacknet_packet_header);
    int i, fragment_number;

    if( pkt_size == sizeof(jacknet_packet_header) )
	    fragment_number = 1;
    else
	    fragment_number = (pkt_size - sizeof (jacknet_packet_header) - 1) / fragment_payload_size + 1;

    packet_cache *pcache = malloc (sizeof (packet_cache));
    if (pcache == NULL)
    {
        jack_error ("could not allocate packet cache (1)\n");
        return NULL;
    }

    pcache->size = num_packets;
    pcache->packets = malloc (sizeof (cache_packet) * num_packets);
    pcache->master_address_valid = 0;
    pcache->last_framecnt_retreived = 0;
    pcache->last_framecnt_retreived_valid = 0;

    if (pcache->packets == NULL)
    {
        jack_error ("could not allocate packet cache (2)\n");
        return NULL;
    }

    for (i = 0; i < num_packets; i++)
    {
        pcache->packets[i].valid = 0;
        pcache->packets[i].num_fragments = fragment_number;
        pcache->packets[i].packet_size = pkt_size;
        pcache->packets[i].mtu = mtu;
        pcache->packets[i].framecnt = 0;
        pcache->packets[i].fragment_array = malloc (sizeof (char) * fragment_number);
        pcache->packets[i].packet_buf = malloc (pkt_size);
        if ((pcache->packets[i].fragment_array == NULL) || (pcache->packets[i].packet_buf == NULL))
        {
            jack_error ("could not allocate packet cache (3)\n");
            return NULL;
        }
    }
    pcache->mtu = mtu;

    return pcache;
}

void
packet_cache_free (packet_cache *pcache)
{
    int i;
    if( pcache == NULL )
	return;

    for (i = 0; i < pcache->size; i++)
    {
        free (pcache->packets[i].fragment_array);
        free (pcache->packets[i].packet_buf);
    }

    free (pcache->packets);
    free (pcache);
}

cache_packet
*packet_cache_get_packet (packet_cache *pcache, jack_nframes_t framecnt)
{
    int i;
    cache_packet *retval;

    for (i = 0; i < pcache->size; i++)
    {
        if (pcache->packets[i].valid && (pcache->packets[i].framecnt == framecnt))
            return &(pcache->packets[i]);
    }

    // The Packet is not in the packet cache.
    // find a free packet.

    retval = packet_cache_get_free_packet (pcache);
    if (retval != NULL)
    {
        cache_packet_set_framecnt (retval, framecnt);
        return retval;
    }

    // No Free Packet available
    // Get The Oldest packet and reset it.

    retval = packet_cache_get_oldest_packet (pcache);
    //printf( "Dropping %d from Cache :S\n", retval->framecnt );
    cache_packet_reset (retval);
    cache_packet_set_framecnt (retval, framecnt);

    return retval;
}

// TODO: fix wrapping case... need to pass
//       current expected frame here.
//
//       or just save framecount into packet_cache.

cache_packet
*packet_cache_get_oldest_packet (packet_cache *pcache)
{
    jack_nframes_t minimal_frame = JACK_MAX_FRAMES;
    cache_packet *retval = &(pcache->packets[0]);
    int i;

    for (i = 0; i < pcache->size; i++)
    {
        if (pcache->packets[i].valid && (pcache->packets[i].framecnt < minimal_frame))
        {
            minimal_frame = pcache->packets[i].framecnt;
            retval = &(pcache->packets[i]);
        }
    }

    return retval;
}

cache_packet
*packet_cache_get_free_packet (packet_cache *pcache)
{
    int i;

    for (i = 0; i < pcache->size; i++)
    {
        if (pcache->packets[i].valid == 0)
            return &(pcache->packets[i]);
    }

    return NULL;
}

void
cache_packet_reset (cache_packet *pack)
{
    int i;
    pack->valid = 0;

    // XXX: i dont think this is necessary here...
    //      fragement array is cleared in _set_framecnt()

    for (i = 0; i < pack->num_fragments; i++)
        pack->fragment_array[i] = 0;
}

void
cache_packet_set_framecnt (cache_packet *pack, jack_nframes_t framecnt)
{
    int i;

    pack->framecnt = framecnt;

    for (i = 0; i < pack->num_fragments; i++)
        pack->fragment_array[i] = 0;

    pack->valid = 1;
}

void
cache_packet_add_fragment (cache_packet *pack, char *packet_buf, int rcv_len)
{
    jacknet_packet_header *pkthdr = (jacknet_packet_header *) packet_buf;
    int fragment_payload_size = pack->mtu - sizeof (jacknet_packet_header);
    char *packet_bufX = pack->packet_buf + sizeof (jacknet_packet_header);
    char *dataX = packet_buf + sizeof (jacknet_packet_header);

    jack_nframes_t fragment_nr = ntohl (pkthdr->fragment_nr);
    jack_nframes_t framecnt    = ntohl (pkthdr->framecnt);

    if (framecnt != pack->framecnt)
    {
        jack_error ("errror. framecnts dont match\n");
        return;
    }


    if (fragment_nr == 0)
    {
        memcpy (pack->packet_buf, packet_buf, rcv_len);
        pack->fragment_array[0] = 1;

        return;
    }

    if ((fragment_nr < pack->num_fragments) && (fragment_nr > 0))
    {
        if ((fragment_nr * fragment_payload_size + rcv_len - sizeof (jacknet_packet_header)) <= (pack->packet_size - sizeof (jacknet_packet_header)))
        {
            memcpy (packet_bufX + fragment_nr * fragment_payload_size, dataX, rcv_len - sizeof (jacknet_packet_header));
            pack->fragment_array[fragment_nr] = 1;
        }
        else
            jack_error ("too long packet received...");
    }
}

int
cache_packet_is_complete (cache_packet *pack)
{
    int i;
    for (i = 0; i < pack->num_fragments; i++)
        if (pack->fragment_array[i] == 0)
            return 0;

    return 1;
}

#ifndef WIN32
// new poll using nanoseconds resolution and
// not waiting forever.
int
netjack_poll_deadline (int sockfd, jack_time_t deadline)
{
    struct pollfd fds;
    int i, poll_err = 0;
    sigset_t sigmask;
    struct sigaction action;
#if HAVE_PPOLL
    struct timespec timeout_spec = { 0, 0 };
#else
    sigset_t rsigmask;
    int timeout;
#endif


    jack_time_t now = jack_get_time();
    if( now >= deadline )
	return 0;

#if HAVE_PPOLL
    timeout_spec.tv_nsec = (deadline - now) * 1000;
#else
    timeout = lrintf( (float)(deadline - now) / 1000.0 );
#endif

    sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGHUP);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGPIPE);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	sigaddset(&sigmask, SIGUSR2);

	action.sa_handler = SIG_DFL;
	action.sa_mask = sigmask;
	action.sa_flags = SA_RESTART;

    for (i = 1; i < NSIG; i++)
        if (sigismember (&sigmask, i))
            sigaction (i, &action, 0);

    fds.fd = sockfd;
    fds.events = POLLIN;

#if HAVE_PPOLL
    poll_err = ppoll (&fds, 1, &timeout_spec, &sigmask);
#else
    sigprocmask (SIG_UNBLOCK, &sigmask, &rsigmask);
    poll_err = poll (&fds, 1, timeout);
    sigprocmask (SIG_SETMASK, &rsigmask, NULL);
#endif

    if (poll_err == -1)
    {
        switch (errno)
        {
            case EBADF:
            jack_error ("Error %d: An invalid file descriptor was given in one of the sets", errno);
            break;
            case EFAULT:
            jack_error ("Error %d: The array given as argument was not contained in the calling program's address space", errno);
            break;
            case EINTR:
            jack_error ("Error %d: A signal occurred before any requested event", errno);
            break;
            case EINVAL:
            jack_error ("Error %d: The nfds value exceeds the RLIMIT_NOFILE value", errno);
            break;
            case ENOMEM:
            jack_error ("Error %d: There was no space to allocate file descriptor tables", errno);
            break;
        }
    }
    return poll_err;
}

int
netjack_poll (int sockfd, int timeout)
{
    struct pollfd fds;
    int i, poll_err = 0;
    sigset_t sigmask, rsigmask;
    struct sigaction action;

    sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGHUP);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGPIPE);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	sigaddset(&sigmask, SIGUSR2);

	action.sa_handler = SIG_DFL;
	action.sa_mask = sigmask;
	action.sa_flags = SA_RESTART;

    for (i = 1; i < NSIG; i++)
        if (sigismember (&sigmask, i))
            sigaction (i, &action, 0);

    fds.fd = sockfd;
    fds.events = POLLIN;

    sigprocmask(SIG_UNBLOCK, &sigmask, &rsigmask);
    while (poll_err == 0)
    {
        poll_err = poll (&fds, 1, timeout);
    }
    sigprocmask(SIG_SETMASK, &rsigmask, NULL);

    if (poll_err == -1)
    {
        switch (errno)
        {
            case EBADF:
            jack_error ("Error %d: An invalid file descriptor was given in one of the sets", errno);
            break;
            case EFAULT:
            jack_error ("Error %d: The array given as argument was not contained in the calling program's address space", errno);
            break;
            case EINTR:
            jack_error ("Error %d: A signal occurred before any requested event", errno);
            break;
            case EINVAL:
            jack_error ("Error %d: The nfds value exceeds the RLIMIT_NOFILE value", errno);
            break;
            case ENOMEM:
            jack_error ("Error %d: There was no space to allocate file descriptor tables", errno);
            break;
        }
        return 0;
    }
    return 1;
}

#else
int
netjack_poll (int sockfd, int timeout)
{
    jack_error( "netjack_poll not implemented\n" );
    return 0;
}
int
netjack_poll_deadline (int sockfd, jack_time_t deadline)
{
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( sockfd, &fds );

    struct timeval timeout;
    while( 1 ) {
        jack_time_t now = jack_get_time();
        if( now >= deadline )
                return 0;

        int timeout_usecs = (deadline - now);
    //jack_error( "timeout = %d", timeout_usecs );
        timeout.tv_sec = 0;
        timeout.tv_usec = (timeout_usecs < 500) ? 500 : timeout_usecs;

        int poll_err = select (0, &fds, NULL, NULL, &timeout);
        if( poll_err != 0 )
            return poll_err;
    }

    return 0;
}
#endif
// This now reads all a socket has into the cache.
// replacing netjack_recv functions.

void
packet_cache_drain_socket( packet_cache *pcache, int sockfd )
{
    char *rx_packet = alloca (pcache->mtu);
    jacknet_packet_header *pkthdr = (jacknet_packet_header *) rx_packet;
    int rcv_len;
    jack_nframes_t framecnt;
    cache_packet *cpack;
    struct sockaddr_in sender_address;
#ifdef WIN32
    size_t senderlen = sizeof( struct sockaddr_in );
    u_long parm = 1;
    ioctlsocket( sockfd, FIONBIO, &parm );
#else
    socklen_t senderlen = sizeof( struct sockaddr_in );
#endif
    jack_log( "drain...." );
    while (1)
    {
        rcv_len = recvfrom (sockfd, rx_packet, pcache->mtu, 0,
			    (struct sockaddr*) &sender_address, &senderlen);
        if (rcv_len < 0)
            return;

	if (pcache->master_address_valid) {
	    // Verify its from our master.
	    if (memcmp (&sender_address, &(pcache->master_address), senderlen) != 0)
		continue;
	} else {
	    // Setup this one as master
	    //printf( "setup master...\n" );
	    memcpy ( &(pcache->master_address), &sender_address, senderlen );
	    pcache->master_address_valid = 1;
	}

        framecnt = ntohl (pkthdr->framecnt);
	if( pcache->last_framecnt_retreived_valid && (framecnt <= pcache->last_framecnt_retreived ))
	    continue;

	jack_log( "Got Packet %d\n", framecnt );
        cpack = packet_cache_get_packet (global_packcache, framecnt);
        cache_packet_add_fragment (cpack, rx_packet, rcv_len);
	cpack->recv_timestamp = jack_get_time();
    }
}

void
packet_cache_reset_master_address( packet_cache *pcache )
{
    pcache->master_address_valid = 0;
    pcache->last_framecnt_retreived = 0;
    pcache->last_framecnt_retreived_valid = 0;
}

void
packet_cache_clear_old_packets (packet_cache *pcache, jack_nframes_t framecnt )
{
    int i;

    for (i = 0; i < pcache->size; i++)
    {
        if (pcache->packets[i].valid && (pcache->packets[i].framecnt < framecnt))
        {
            cache_packet_reset (&(pcache->packets[i]));
        }
    }
}

int
packet_cache_retreive_packet_pointer( packet_cache *pcache, jack_nframes_t framecnt, char **packet_buf, int pkt_size, jack_time_t *timestamp )
{
    int i;
    cache_packet *cpack = NULL;


    for (i = 0; i < pcache->size; i++) {
        if (pcache->packets[i].valid && (pcache->packets[i].framecnt == framecnt)) {
	    cpack = &(pcache->packets[i]);
            break;
	}
    }

    if( cpack == NULL ) {
	//printf( "retreive packet: %d....not found\n", framecnt );
	return -1;
    }

    if( !cache_packet_is_complete( cpack ) ) {
	return -1;
    }

    // ok. cpack is the one we want and its complete.
    *packet_buf = cpack->packet_buf;
    if( timestamp )
	*timestamp = cpack->recv_timestamp;

    pcache->last_framecnt_retreived_valid = 1;
    pcache->last_framecnt_retreived = framecnt;

    return pkt_size;
}

int
packet_cache_release_packet( packet_cache *pcache, jack_nframes_t framecnt )
{
    int i;
    cache_packet *cpack = NULL;


    for (i = 0; i < pcache->size; i++) {
        if (pcache->packets[i].valid && (pcache->packets[i].framecnt == framecnt)) {
	    cpack = &(pcache->packets[i]);
            break;
	}
    }

    if( cpack == NULL ) {
	//printf( "retreive packet: %d....not found\n", framecnt );
	return -1;
    }

    if( !cache_packet_is_complete( cpack ) ) {
	return -1;
    }

    cache_packet_reset (cpack);
    packet_cache_clear_old_packets( pcache, framecnt );

    return 0;
}
float
packet_cache_get_fill( packet_cache *pcache, jack_nframes_t expected_framecnt )
{
    int num_packets_before_us = 0;
    int i;

    for (i = 0; i < pcache->size; i++)
    {
	cache_packet *cpack = &(pcache->packets[i]);
        if (cpack->valid && cache_packet_is_complete( cpack ))
	    if( cpack->framecnt >= expected_framecnt )
		num_packets_before_us += 1;
    }

    return 100.0 * (float)num_packets_before_us / (float)( pcache->size ) ;
}

// Returns 0 when no valid packet is inside the cache.
int
packet_cache_get_next_available_framecnt( packet_cache *pcache, jack_nframes_t expected_framecnt, jack_nframes_t *framecnt )
{
    int i;
    jack_nframes_t best_offset = JACK_MAX_FRAMES/2-1;
    int retval = 0;

    for (i = 0; i < pcache->size; i++)
    {
	cache_packet *cpack = &(pcache->packets[i]);
	//printf( "p%d: valid=%d, frame %d\n", i, cpack->valid, cpack->framecnt );

        if (!cpack->valid || !cache_packet_is_complete( cpack )) {
	    //printf( "invalid\n" );
	    continue;
	}

	if( (cpack->framecnt - expected_framecnt) > best_offset ) {
	    continue;
	}

	best_offset = cpack->framecnt - expected_framecnt;
	retval = 1;

	if( best_offset == 0 )
	    break;
    }
    if( retval && framecnt )
	*framecnt = expected_framecnt + best_offset;

    return retval;
}

int
packet_cache_get_highest_available_framecnt( packet_cache *pcache, jack_nframes_t *framecnt )
{
    int i;
    jack_nframes_t best_value = 0;
    int retval = 0;

    for (i = 0; i < pcache->size; i++)
    {
	cache_packet *cpack = &(pcache->packets[i]);
	//printf( "p%d: valid=%d, frame %d\n", i, cpack->valid, cpack->framecnt );

        if (!cpack->valid || !cache_packet_is_complete( cpack )) {
	    //printf( "invalid\n" );
	    continue;
	}

	if (cpack->framecnt < best_value) {
	    continue;
	}

	best_value = cpack->framecnt;
	retval = 1;

    }
    if( retval && framecnt )
	*framecnt = best_value;

    return retval;
}

// Returns 0 when no valid packet is inside the cache.
int
packet_cache_find_latency( packet_cache *pcache, jack_nframes_t expected_framecnt, jack_nframes_t *framecnt )
{
    int i;
    jack_nframes_t best_offset = 0;
    int retval = 0;

    for (i = 0; i < pcache->size; i++)
    {
	cache_packet *cpack = &(pcache->packets[i]);
	//printf( "p%d: valid=%d, frame %d\n", i, cpack->valid, cpack->framecnt );

        if (!cpack->valid || !cache_packet_is_complete( cpack )) {
	    //printf( "invalid\n" );
	    continue;
	}

	if( (cpack->framecnt - expected_framecnt) < best_offset ) {
	    continue;
	}

	best_offset = cpack->framecnt - expected_framecnt;
	retval = 1;

	if( best_offset == 0 )
	    break;
    }
    if( retval && framecnt )
	*framecnt = JACK_MAX_FRAMES - best_offset;

    return retval;
}
// fragmented packet IO
int
netjack_recvfrom (int sockfd, char *packet_buf, int pkt_size, int flags, struct sockaddr *addr, size_t *addr_size, int mtu)
{
    if (pkt_size <= mtu)
        return recvfrom (sockfd, packet_buf, pkt_size, flags, addr, addr_size);
    char *rx_packet = alloca (mtu);
    jacknet_packet_header *pkthdr = (jacknet_packet_header *) rx_packet;
    int rcv_len;
    jack_nframes_t framecnt;
    cache_packet *cpack;
    do
    {
        rcv_len = recvfrom (sockfd, rx_packet, mtu, 0, addr, addr_size);
        if (rcv_len < 0)
            return rcv_len;
        framecnt = ntohl (pkthdr->framecnt);
        cpack = packet_cache_get_packet (global_packcache, framecnt);
        cache_packet_add_fragment (cpack, rx_packet, rcv_len);
    } while (!cache_packet_is_complete (cpack));
    memcpy (packet_buf, cpack->packet_buf, pkt_size);
    cache_packet_reset (cpack);
    return pkt_size;
}

int
netjack_recv (int sockfd, char *packet_buf, int pkt_size, int flags, int mtu)
{
    if (pkt_size <= mtu)
        return recv (sockfd, packet_buf, pkt_size, flags);
    char *rx_packet = alloca (mtu);
    jacknet_packet_header *pkthdr = (jacknet_packet_header *) rx_packet;
    int rcv_len;
    jack_nframes_t framecnt;
    cache_packet *cpack;
    do
    {
        rcv_len = recv (sockfd, rx_packet, mtu, flags);
        if (rcv_len < 0)
            return rcv_len;
        framecnt = ntohl (pkthdr->framecnt);
        cpack = packet_cache_get_packet (global_packcache, framecnt);
        cache_packet_add_fragment (cpack, rx_packet, rcv_len);
    } while (!cache_packet_is_complete (cpack));
    memcpy (packet_buf, cpack->packet_buf, pkt_size);
    cache_packet_reset (cpack);
    return pkt_size;
}

void
netjack_sendto (int sockfd, char *packet_buf, int pkt_size, int flags, struct sockaddr *addr, int addr_size, int mtu)
{
    int frag_cnt = 0;
    char *tx_packet, *dataX;
    jacknet_packet_header *pkthdr;

    tx_packet = alloca (mtu + 10);
    dataX = tx_packet + sizeof (jacknet_packet_header);
    pkthdr = (jacknet_packet_header *) tx_packet;

    int fragment_payload_size = mtu - sizeof (jacknet_packet_header);

    if (pkt_size <= mtu) {
	int err;
	pkthdr = (jacknet_packet_header *) packet_buf;
        pkthdr->fragment_nr = htonl (0);
        err = sendto(sockfd, packet_buf, pkt_size, flags, addr, addr_size);
	if( err<0 ) {
	    printf( "error in send\n" );
	    perror( "send" );
	}
    }
    else
    {
	int err;
        // Copy the packet header to the tx pack first.
        memcpy(tx_packet, packet_buf, sizeof (jacknet_packet_header));

        // Now loop and send all
        char *packet_bufX = packet_buf + sizeof (jacknet_packet_header);

        while (packet_bufX < (packet_buf + pkt_size - fragment_payload_size))
        {
            pkthdr->fragment_nr = htonl (frag_cnt++);
            memcpy (dataX, packet_bufX, fragment_payload_size);
            sendto (sockfd, tx_packet, mtu, flags, addr, addr_size);
            packet_bufX += fragment_payload_size;
        }

        int last_payload_size = packet_buf + pkt_size - packet_bufX;
        memcpy (dataX, packet_bufX, last_payload_size);
        pkthdr->fragment_nr = htonl (frag_cnt);
        //jack_error("last fragment_count = %d, payload_size = %d\n", fragment_count, last_payload_size);

        // sendto(last_pack_size);
        err = sendto(sockfd, tx_packet, last_payload_size + sizeof(jacknet_packet_header), flags, addr, addr_size);
	if( err<0 ) {
	    printf( "error in send\n" );
	    perror( "send" );
	}
    }
}


void
decode_midi_buffer (uint32_t *buffer_uint32, unsigned int buffer_size_uint32, jack_default_audio_sample_t* buf)
{
    int i;
    jack_midi_clear_buffer (buf);
    for (i = 0; i < buffer_size_uint32 - 3;)
    {
        uint32_t payload_size;
        payload_size = buffer_uint32[i];
        payload_size = ntohl (payload_size);
        if (payload_size)
        {
            jack_midi_event_t event;
            event.time = ntohl (buffer_uint32[i+1]);
            event.size = ntohl (buffer_uint32[i+2]);
            event.buffer = (jack_midi_data_t*) (&(buffer_uint32[i+3]));
            jack_midi_event_write (buf, event.time, event.buffer, event.size);

            // skip to the next event
            unsigned int nb_data_quads = (((event.size-1) & ~0x3) >> 2)+1;
            i += 3+nb_data_quads;
        }
        else
            break; // no events can follow an empty event, we're done
    }
}

void
encode_midi_buffer (uint32_t *buffer_uint32, unsigned int buffer_size_uint32, jack_default_audio_sample_t* buf)
{
    int i;
    unsigned int written = 0;
    // midi port, encode midi events
    unsigned int nevents = jack_midi_get_event_count (buf);
    for (i = 0; i < nevents; ++i)
    {
        jack_midi_event_t event;
        jack_midi_event_get (&event, buf, i);
        unsigned int nb_data_quads = (((event.size - 1) & ~0x3) >> 2) + 1;
        unsigned int payload_size = 3 + nb_data_quads;
        // only write if we have sufficient space for the event
        // otherwise drop it
        if (written + payload_size < buffer_size_uint32 - 1)
        {
            // write header
            buffer_uint32[written]=htonl (payload_size);
            written++;
            buffer_uint32[written]=htonl (event.time);
            written++;
            buffer_uint32[written]=htonl (event.size);
            written++;

            // write data
            jack_midi_data_t* tmpbuff = (jack_midi_data_t*)(&(buffer_uint32[written]));
            memcpy (tmpbuff, event.buffer, event.size);
            written += nb_data_quads;
        }
        else
        {
            // buffer overflow
            jack_error ("midi buffer overflow");
            break;
        }
    }
    // now put a netjack_midi 'no-payload' event, signaling EOF
    buffer_uint32[written]=0;
}
