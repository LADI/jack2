/*
    Copyright (C) 2004 Ian Esten

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <jack/jack.h>
#include <jack/midiport.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

jack_client_t *client;
jack_port_t *output_port;

jack_transport_state_t old_trans_state;
float bpm;
int last_tick_pos = 0;
int clock_rolling = 0;
static void signal_handler(int sig)
{
	jack_client_close(client);
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
}

static void usage()
{
	fprintf(stderr, "usage: jack_midiseq name nsamp [startindex note nsamp] ...... [startindex note nsamp]\n");
	fprintf(stderr, "eg: jack_midiseq Sequencer 24000 0 60 8000 12000 63 8000\n");
	fprintf(stderr, "will play a 1/2 sec loop (if srate is 48khz) with a c4 note at the start of the loop\n");
	fprintf(stderr, "that lasts for 12000 samples, then a d4# that starts at 1/4 sec that lasts for 800 samples\n");
}
double ceil( double );

static int process(jack_nframes_t nframes, void *arg)
{
	int i,j;
	void* port_buf = jack_port_get_buffer(output_port, nframes);
	unsigned char* buffer;
	jack_midi_clear_buffer(port_buf);

	jack_position_t ttime;
	jack_transport_state_t trans_state;

	trans_state = jack_transport_query( client, &ttime );

	double position_secs = (double) ttime.frame / (double) ttime.frame_rate;
	double beat_float = position_secs * (double) bpm / 60.0;
	double tick_float = beat_float * (double) 24.0;

	double ticks_to_next_tick = ceil( tick_float ) - tick_float;
        jack_nframes_t frames_to_next_tick  = (jack_nframes_t) (ticks_to_next_tick / (double) 24.0 / (double) bpm * 60.0 * (double) ttime.frame_rate );

	jack_nframes_t frames_per_tick = (jack_nframes_t) ((double) ttime.frame_rate * 60.0 / (double) bpm / 24.0);
	if( trans_state == JackTransportRolling )
	{
		if( old_trans_state != JackTransportRolling )
		{

			// transport just started... emit start or cont
			buffer = jack_midi_event_reserve(port_buf, 0, 1);
			if( ttime.frame == 0 )
				buffer[0] = 0xfa;  // start
			else
				buffer[0] = 0xfb;  // cont.

		}

		// transport rolling ... emit clocks.
		jack_nframes_t emit_frame = frames_to_next_tick;
		int next_tick = ceil( tick_float );

        if( !clock_rolling ) {
            if( emit_frame < nframes ) {
                if( (next_tick % 6) == 0 ) {
                    // ok... we are at an SPP postion.
                    // now emit that, and continue.
  				buffer = jack_midi_event_reserve(port_buf, emit_frame, 3);
				buffer[0] = 0xf2;
				buffer[1] = (next_tick/6) & 0x7f;
				buffer[2] = ((next_tick/6) >> 7) & 0x7f;

                buffer = jack_midi_event_reserve(port_buf, emit_frame, 1);
                buffer[0] = 0xfb;  // cont.

                clock_rolling = 1;
                emit_frame += frames_per_tick;
                next_tick  += 1;

                }
            }

        }
		//jack_error( "rolling.... next_tick (%d) in %d fames_per_tick %d", next_tick, frames_to_next_tick, frames_per_tick );
        if( clock_rolling ) {
		while( emit_frame < nframes )
		{
			buffer = jack_midi_event_reserve(port_buf, emit_frame, 1);
			buffer[0] = 0xf8;

#if 0
			if( (next_tick % 6) == 0 )
			{
				buffer = jack_midi_event_reserve(port_buf, emit_frame, 3);
				buffer[0] = 0xf2;
				buffer[1] = (next_tick/6) & 0x7f;
				buffer[2] = ((next_tick/6) >> 7) & 0x7f;
			}
#endif
			emit_frame += frames_per_tick;
			next_tick  += 1;
		}
        }
	}
	else
	{
	    clock_rolling = 0;
		if( old_trans_state == JackTransportRolling )
		{
			// transport has stopped.
			// emit MidiSTOP.

			buffer = jack_midi_event_reserve(port_buf, 0, 1);
			buffer[0] = 0xfc;  // stop
            // make the code trigger a SPP emission.
            last_tick_pos = (int) 0;

		}
		else
		{
			// transport is still stopped....
			// but emitting SPP of current position might make sense.
                        if( last_tick_pos != (int) tick_float )
                        {
                                last_tick_pos = (int) tick_float;
                                buffer = jack_midi_event_reserve(port_buf, 0, 3);
                                buffer[0] = 0xf2;
                                buffer[1] = (last_tick_pos/6) & 0x7f;
                                buffer[2] = ((last_tick_pos/6) >> 7) & 0x7f;
                        }
		}
	}

	old_trans_state = trans_state;
	return 0;
}

int main(int narg, char **args)
{
	int i;
	jack_nframes_t nframes;
	if( narg != 2)
	{
		usage();
		exit(1);
	}

	bpm = atof( args[1] );
	if( bpm < 1.0 )
		bpm = 120.0;

	if((client = jack_client_open ("trans2midi", JackNullOption, NULL)) == 0)
	{
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}
	jack_set_process_callback (client, process, 0);
	output_port = jack_port_register (client, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

	if (jack_activate(client))
	{
		fprintf (stderr, "cannot activate client");
		return 1;
	}

    /* install a signal handler to properly quits jack client */
        signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
#ifdef SIGQUIT
        signal(SIGQUIT, signal_handler);
	signal(SIGHUP, signal_handler);
#endif

    /* run until interrupted */
	while (1) {
#ifdef WIN32
                Sleep(1000);
#else
		sleep(1);
#endif
	};

    jack_client_close(client);
	exit (0);
}
