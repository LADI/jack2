/*
Copyright (C) 2001 Paul Davis
Copyright (C) 2008 Romain Moret at Grame

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


#include "JackNetOneDriver.h"
#include "JackEngineControl.h"
#include "JackGraphManager.h"
#include "JackWaitThreadedDriver.h"
#include "driver_interface.h"

#include "netjack.h"
#include "netjack_packet.h"

#if HAVE_SAMPLERATE
#include "samplerate.h"
#endif

#define MIN(x,y) ((x)<(y) ? (x) : (y))

using namespace std;

namespace Jack
{
    JackNetOneDriver::JackNetOneDriver ( const char* name, const char* alias, JackLockedEngine* engine, JackSynchro* table,
                                   const char* ip, int port, int mtu, int capture_ports, int playback_ports, int midi_input_ports, int midi_output_ports,
                                   char* net_name, uint transport_sync, char network_mode )
            : JackAudioDriver ( name, alias, engine, table )
    {
        jack_log ( "JackNetOneDriver::JackNetOneDriver ip %s, port %d", ip, port );

	netjack_init( & (this->netj),
		NULL, // client
                name,
                capture_ports,
                playback_ports,
                midi_input_ports,
                midi_output_ports,
                44100, //sample_rate,
                512, //period_size,
                port,
                transport_sync,
                1,
                1,
                0, //bitdepth,
		1, //use_autoconfig,
		2, //latency,
		1, //redundancy,
		0 ); //dont_htonl_floats);
    }

    JackNetOneDriver::~JackNetOneDriver()
    {
	// No destructor yet.
    }

//open, close, attach and detach------------------------------------------------------
    int JackNetOneDriver::Open ( jack_nframes_t buffer_size, jack_nframes_t samplerate, bool capturing, bool playing,
                              int inchannels, int outchannels, bool monitor,
                              const char* capture_driver_name, const char* playback_driver_name,
                              jack_nframes_t capture_latency, jack_nframes_t playback_latency )
    {
        if ( JackAudioDriver::Open ( buffer_size,
                                     samplerate,
                                     capturing,
                                     playing,
                                     inchannels,
                                     outchannels,
                                     monitor,
                                     capture_driver_name,
                                     playback_driver_name,
                                     capture_latency,
                                     playback_latency ) == 0 )
        {
            fEngineControl->fPeriod = 0;
            fEngineControl->fComputation = 500 * 1000;
            fEngineControl->fConstraint = 500 * 1000;
            return 0;
        }
        else
        {
            return -1;
        }
    }

    int JackNetOneDriver::Attach()
    {
	jack_port_id_t port_id;
	JackPort *port;
	char buf[32];
	unsigned int chn;
	int port_flags;


	//if (netj.handle_transport_sync)
	//    jack_set_sync_callback(netj.client, (JackSyncCallback) net_driver_sync_cb, NULL);

	port_flags = JackPortIsOutput | JackPortIsPhysical | JackPortIsTerminal;

	for (chn = 0; chn < netj.capture_channels_audio; chn++) {
	    snprintf (buf, sizeof(buf) - 1, "capture_%u", chn + 1);

            if ( ( port_id = fGraphManager->AllocatePort ( fClientControl.fRefNum, buf, JACK_DEFAULT_AUDIO_TYPE,
                             static_cast<JackPortFlags> ( port_flags ), fEngineControl->fBufferSize ) ) == NO_PORT )
            {
                jack_error ( "driver: cannot register port for %s", buf );
                return -1;
            }
            //port = fGraphManager->GetPort ( port_id );

	    netj.capture_ports =
		jack_slist_append (netj.capture_ports, (void *)(intptr_t)port_id);

	    if( netj.bitdepth == 1000 ) {
#if HAVE_CELT
		celt_int32_t lookahead;
		// XXX: memory leak
		CELTMode *celt_mode = celt_mode_create( netj.sample_rate, 1, netj.period_size, NULL );
		celt_mode_info( celt_mode, CELT_GET_LOOKAHEAD, &lookahead );
		netj.codec_latency = 2*lookahead;

		netj.capture_srcs = jack_slist_append(netj.capture_srcs, (void *)celt_decoder_create( celt_mode ) );
#endif
	    } else {
#if HAVE_SAMPLERATE 
		netj.capture_srcs = jack_slist_append(netj.capture_srcs, (void *)src_new(SRC_LINEAR, 1, NULL));
#endif
	    }
	}
	for (chn = netj.capture_channels_audio; chn < netj.capture_channels; chn++) {
	    snprintf (buf, sizeof(buf) - 1, "capture_%u", chn + 1);

            if ( ( port_id = fGraphManager->AllocatePort ( fClientControl.fRefNum, buf, JACK_DEFAULT_MIDI_TYPE,
                             static_cast<JackPortFlags> ( port_flags ), fEngineControl->fBufferSize ) ) == NO_PORT )
            {
                jack_error ( "driver: cannot register port for %s", buf );
                return -1;
            }
            //port = fGraphManager->GetPort ( port_id );

	    netj.capture_ports =
		jack_slist_append (netj.capture_ports, (void *)(intptr_t)port_id);
	}

	port_flags = JackPortIsInput | JackPortIsPhysical | JackPortIsTerminal;

	for (chn = 0; chn < netj.playback_channels_audio; chn++) {
	    snprintf (buf, sizeof(buf) - 1, "playback_%u", chn + 1);

            if ( ( port_id = fGraphManager->AllocatePort ( fClientControl.fRefNum, buf, JACK_DEFAULT_AUDIO_TYPE,
                             static_cast<JackPortFlags> ( port_flags ), fEngineControl->fBufferSize ) ) == NO_PORT )
            {
                jack_error ( "driver: cannot register port for %s", buf );
                return -1;
            }
            //port = fGraphManager->GetPort ( port_id );

	    netj.playback_ports =
		jack_slist_append (netj.playback_ports, (void *)(intptr_t)port_id);

	    if( netj.bitdepth == 1000 ) {
#if HAVE_CELT
		// XXX: memory leak
		CELTMode *celt_mode = celt_mode_create( netj.sample_rate, 1, netj.period_size, NULL );
		netj.playback_srcs = jack_slist_append(netj.playback_srcs, (void *)celt_encoder_create( celt_mode ) );
#endif
	    } else {
#if HAVE_SAMPLERATE
		netj.playback_srcs = jack_slist_append(netj.playback_srcs, (void *)src_new(SRC_LINEAR, 1, NULL));
#endif
	    }
	}
	for (chn = netj.playback_channels_audio; chn < netj.playback_channels; chn++) {
	    snprintf (buf, sizeof(buf) - 1, "playback_%u", chn + 1);

            if ( ( port_id = fGraphManager->AllocatePort ( fClientControl.fRefNum, buf, JACK_DEFAULT_MIDI_TYPE,
                             static_cast<JackPortFlags> ( port_flags ), fEngineControl->fBufferSize ) ) == NO_PORT )
            {
                jack_error ( "driver: cannot register port for %s", buf );
                return -1;
            }
            //port = fGraphManager->GetPort ( port_id );

	    netj.playback_ports =
		jack_slist_append (netj.playback_ports, (void *)(intptr_t)port_id);
	}

	jack_activate (netj.client);
	return 0;
    }

    int JackNetOneDriver::Detach()
    {
        return 0;
    }

//init and restart--------------------------------------------------------------------
    bool JackNetOneDriver::Init()
    {
        jack_log ( "JackNetOneDriver::Init()" );


        //display some additional infos
        jack_info ( "NetOne driver started" );

        //register jack ports
//        if ( AllocPorts() != 0 )
//        {
//            jack_error ( "Can't allocate ports." );
//            return false;
//        }


        //monitor
        //driver parametering
        JackAudioDriver::SetBufferSize ( netj.period_size );
        JackAudioDriver::SetSampleRate ( netj.sample_rate );

        JackDriver::NotifyBufferSize ( netj.period_size );
        JackDriver::NotifySampleRate ( netj.sample_rate );

        //transport engine parametering
        fEngineControl->fTransport.SetNetworkSync ( true );
        return true;
    }


//jack ports and buffers--------------------------------------------------------------

//driver processes--------------------------------------------------------------------
    int JackNetOneDriver::Read()
    {
	netjack_wait( &netj );
	//netjack_read( &netj, netj.period_size );
        JackDriver::CycleTakeBeginTime();

	jack_position_t local_trans_pos;
	jack_transport_state_t local_trans_state;

	unsigned int *packet_buf, *packet_bufX;

	if( ! netj.packet_data_valid ) {
	    render_payload_to_jack_ports (netj.bitdepth, NULL, netj.net_period_down, netj.capture_ports, netj.capture_srcs, netj.period_size, netj.dont_htonl_floats );
	    return 0;
	}
	packet_buf = netj.rx_buf;

	jacknet_packet_header *pkthdr = (jacknet_packet_header *)packet_buf;

	packet_bufX = packet_buf + sizeof(jacknet_packet_header) / sizeof(jack_default_audio_sample_t);

	netj.reply_port = pkthdr->reply_port;
	netj.latency = pkthdr->latency;

	// Special handling for latency=0
	if( netj.latency == 0 )
	    netj.resync_threshold = 0;
	else
	    netj.resync_threshold = MIN( 15, pkthdr->latency-1 );

	// check whether, we should handle the transport sync stuff, or leave trnasports untouched.
//	if (netj.handle_transport_sync) {
#if 0
	    int compensated_tranport_pos = (pkthdr->transport_frame + (pkthdr->latency * netj.period_size) + netj.codec_latency);

	    // read local transport info....
	    local_trans_state = jack_transport_query(netj.client, &local_trans_pos);

	    // Now check if we have to start or stop local transport to sync to remote...
	    switch (pkthdr->transport_state) {
		case JackTransportStarting:
		    // the master transport is starting... so we set our reply to the sync_callback;
		    if (local_trans_state == JackTransportStopped) {
			jack_transport_start(netj.client);
			last_transport_state = JackTransportStopped;
			sync_state = 0;
			jack_info("locally stopped... starting...");
		    }

		    if (local_trans_pos.frame != compensated_tranport_pos)
		    {
			jack_transport_locate(netj.client, compensated_tranport_pos);
			last_transport_state = JackTransportRolling;
			sync_state = 0;
			jack_info("starting locate to %d", compensated_tranport_pos );
		    }
		    break;
		case JackTransportStopped:
		    sync_state = 1;
		    if (local_trans_pos.frame != (pkthdr->transport_frame)) {
			jack_transport_locate(netj.client, (pkthdr->transport_frame));
			jack_info("transport is stopped locate to %d", pkthdr->transport_frame);
		    }
		    if (local_trans_state != JackTransportStopped)
			jack_transport_stop(netj.client);
		    break;
		case JackTransportRolling:
		    sync_state = 1;
		    //		    		if(local_trans_pos.frame != (pkthdr->transport_frame + (pkthdr->latency) * netj.period_size)) {
		    //				    jack_transport_locate(netj.client, (pkthdr->transport_frame + (pkthdr->latency + 2) * netj.period_size));
		    //				    jack_info("running locate to %d", pkthdr->transport_frame + (pkthdr->latency)*netj.period_size);
		    //		    		}
		    if (local_trans_state != JackTransportRolling)
			jack_transport_start (netj.client);
		    break;

		case JackTransportLooping:
		    break;
	    }
	}
#endif

	render_payload_to_jack_ports (netj.bitdepth, packet_bufX, netj.net_period_down, netj.capture_ports, netj.capture_srcs, netj.period_size, netj.dont_htonl_floats );
	return 0;
    }

    int JackNetOneDriver::Write()
    {
	int syncstate = 1;
	//netjack_write( &netj, netj.period_size, 0 );
	uint32_t *packet_buf, *packet_bufX;

	int packet_size = get_sample_size(netj.bitdepth) * netj.playback_channels * netj.net_period_up + sizeof(jacknet_packet_header);
	jacknet_packet_header *pkthdr; 

	packet_buf = (uint32_t *) alloca(packet_size);
	pkthdr = (jacknet_packet_header *)packet_buf;

	if( netj.running_free ) {
	    return 0;
	}

	// offset packet_bufX by the packetheader.
	packet_bufX = packet_buf + sizeof(jacknet_packet_header) / sizeof(jack_default_audio_sample_t);

	pkthdr->sync_state = syncstate;;
	pkthdr->latency = netj.time_to_deadline;
	//printf( "time to deadline = %d  goodness=%d\n", (int)netj.time_to_deadline, netj.deadline_goodness );
	pkthdr->framecnt = netj.expected_framecnt;


	render_jack_ports_to_payload(netj.bitdepth, netj.playback_ports, netj.playback_srcs, netj.period_size, packet_bufX, netj.net_period_up, netj.dont_htonl_floats );

	packet_header_hton(pkthdr);
	if (netj.srcaddress_valid)
	{
	    int r;

#ifdef __APPLE__
	    static const int flag = 0;
#else
	    static const int flag = MSG_CONFIRM;
#endif

	    if (netj.reply_port)
		netj.syncsource_address.sin_port = htons(netj.reply_port);

	    for( r=0; r<netj.redundancy; r++ )
		netjack_sendto(netj.outsockfd, (char *)packet_buf, packet_size,
			flag, (struct sockaddr*)&(netj.syncsource_address), sizeof(struct sockaddr_in), netj.mtu);
	}
	return 0;
    }

//Render functions--------------------------------------------------------------------

// render functions for float
void
JackNetOneDriver::render_payload_to_jack_ports_float ( void *packet_payload, jack_nframes_t net_period_down, JSList *capture_ports, JSList *capture_srcs, jack_nframes_t nframes, int dont_htonl_floats)
{
    uint32_t chn = 0;
    JSList *node = capture_ports;
#if HAVE_SAMPLERATE 
    JSList *src_node = capture_srcs;
#endif

    uint32_t *packet_bufX = (uint32_t *)packet_payload;

    if( !packet_payload )
	return;

    while (node != NULL)
    {
        int i;
        int_float_t val;
#if HAVE_SAMPLERATE 
        SRC_DATA src;
#endif

	jack_port_id_t port_id = (jack_port_id_t)(intptr_t) node->data;
	JackPort *port = fGraphManager->GetPort( port_id );

        jack_default_audio_sample_t* buf =
            (jack_default_audio_sample_t*)fGraphManager->GetBuffer(port_id, fEngineControl->fBufferSize);

        const char *porttype = port->GetType();

        if (strncmp (porttype, JACK_DEFAULT_AUDIO_TYPE, jack_port_type_size()) == 0)
        {
#if HAVE_SAMPLERATE 
            // audio port, resample if necessary
            if (net_period_down != nframes)
            {
                SRC_STATE *src_state = (SRC_STATE *)src_node->data;
                for (i = 0; i < net_period_down; i++)
                {
                    packet_bufX[i] = ntohl (packet_bufX[i]);
                }
    
                src.data_in = (float *) packet_bufX;
                src.input_frames = net_period_down;
    
                src.data_out = buf;
                src.output_frames = nframes;
    
                src.src_ratio = (float) nframes / (float) net_period_down;
                src.end_of_input = 0;
    
                src_set_ratio (src_state, src.src_ratio);
                src_process (src_state, &src);
                src_node = jack_slist_next (src_node);
            }
            else
#endif
            {
		if( dont_htonl_floats ) 
		{
		    memcpy( buf, packet_bufX, net_period_down*sizeof(jack_default_audio_sample_t));
		}
		else
		{
		    for (i = 0; i < net_period_down; i++)
		    {
			val.i = packet_bufX[i];
			val.i = ntohl (val.i);
			buf[i] = val.f;
		    }
		}
            }
        }
        else if (strncmp (porttype, JACK_DEFAULT_MIDI_TYPE, jack_port_type_size()) == 0)
        {
            // midi port, decode midi events
            // convert the data buffer to a standard format (uint32_t based)
            unsigned int buffer_size_uint32 = net_period_down;
            uint32_t * buffer_uint32 = (uint32_t*)packet_bufX;
            decode_midi_buffer (buffer_uint32, buffer_size_uint32, buf);
        }
        packet_bufX = (packet_bufX + net_period_down);
        node = jack_slist_next (node);
        chn++;
    }
}

void
JackNetOneDriver::render_jack_ports_to_payload_float (JSList *playback_ports, JSList *playback_srcs, jack_nframes_t nframes, void *packet_payload, jack_nframes_t net_period_up, int dont_htonl_floats )
{
    uint32_t chn = 0;
    JSList *node = playback_ports;
#if HAVE_SAMPLERATE
    JSList *src_node = playback_srcs;
#endif

    uint32_t *packet_bufX = (uint32_t *) packet_payload;

    while (node != NULL)
    {
#if HAVE_SAMPLERATE 
        SRC_DATA src;
#endif
        int i;
        int_float_t val;
	jack_port_id_t port_id = (jack_port_id_t)(intptr_t) node->data;
	JackPort *port = fGraphManager->GetPort( port_id );

        jack_default_audio_sample_t* buf =
            (jack_default_audio_sample_t*)fGraphManager->GetBuffer(port_id, fEngineControl->fBufferSize);

        const char *porttype = port->GetType();

        if (strncmp (porttype, JACK_DEFAULT_AUDIO_TYPE, jack_port_type_size()) == 0)
        {
            // audio port, resample if necessary
    
#if HAVE_SAMPLERATE 
            if (net_period_up != nframes) {
                SRC_STATE *src_state = (SRC_STATE *) src_node->data;
                src.data_in = buf;
                src.input_frames = nframes;
    
                src.data_out = (float *) packet_bufX;
                src.output_frames = net_period_up;
    
                src.src_ratio = (float) net_period_up / (float) nframes;
                src.end_of_input = 0;
    
                src_set_ratio (src_state, src.src_ratio);
                src_process (src_state, &src);
    
                for (i = 0; i < net_period_up; i++)
                {
                    packet_bufX[i] = htonl (packet_bufX[i]);
                }
                src_node = jack_slist_next (src_node);
            }
            else
#endif
            {
		if( dont_htonl_floats )
		{
		    memcpy( packet_bufX, buf, net_period_up*sizeof(jack_default_audio_sample_t) );
		}
		else
		{
		    for (i = 0; i < net_period_up; i++)
		    {
			val.f = buf[i];
			val.i = htonl (val.i);
			packet_bufX[i] = val.i;
		    }
		}
            }
        }
        else if (strncmp(porttype, JACK_DEFAULT_MIDI_TYPE, jack_port_type_size()) == 0)
        {
            // encode midi events from port to packet
            // convert the data buffer to a standard format (uint32_t based)
            unsigned int buffer_size_uint32 = net_period_up;
            uint32_t * buffer_uint32 = (uint32_t*) packet_bufX;
            encode_midi_buffer (buffer_uint32, buffer_size_uint32, buf);
        }
        packet_bufX = (packet_bufX + net_period_up);
        node = jack_slist_next (node);
        chn++;
    }
}

#if HAVE_CELT
// render functions for celt.
void
JackNetOneDriver::render_payload_to_jack_ports_celt (void *packet_payload, jack_nframes_t net_period_down, JSList *capture_ports, JSList *capture_srcs, jack_nframes_t nframes)
{
    uint32_t chn = 0;
    JSList *node = capture_ports;
    JSList *src_node = capture_srcs;

    unsigned char *packet_bufX = (unsigned char *)packet_payload;

    while (node != NULL)
    {
	jack_port_id_t port_id = (jack_port_id_t) node->data;
	JackPort *port = fGraphManager->GetPort( port_id );

        jack_default_audio_sample_t* buf =
            (jack_default_audio_sample_t*)fGraphManager->GetBuffer(port_id, fEngineControl->fBufferSize);

        const char *portname = port->GetType();


        if (strncmp(portname, JACK_DEFAULT_AUDIO_TYPE, jack_port_type_size()) == 0)
        {
            // audio port, decode celt data.
	    
	    CELTDecoder *decoder = src_node->data;
	    if( !packet_payload )
		celt_decode_float( decoder, NULL, net_period_down, buf );
	    else
		celt_decode_float( decoder, packet_bufX, net_period_down, buf );

	    src_node = jack_slist_next (src_node);
        }
        else if (strncmp(portname, JACK_DEFAULT_MIDI_TYPE, jack_port_type_size()) == 0)
        {
            // midi port, decode midi events
            // convert the data buffer to a standard format (uint32_t based)
            unsigned int buffer_size_uint32 = net_period_down / 2;
            uint32_t * buffer_uint32 = (uint32_t*) packet_bufX;
	    if( packet_payload )
		decode_midi_buffer (buffer_uint32, buffer_size_uint32, buf);
        }
        packet_bufX = (packet_bufX + net_period_down);
        node = jack_slist_next (node);
        chn++;
    }
}

void
JackNetOneDriver::render_jack_ports_to_payload_celt (JSList *playback_ports, JSList *playback_srcs, jack_nframes_t nframes, void *packet_payload, jack_nframes_t net_period_up)
{
    uint32_t chn = 0;
    JSList *node = playback_ports;
    JSList *src_node = playback_srcs;

    unsigned char *packet_bufX = (unsigned char *)packet_payload;

    while (node != NULL)
    {
	jack_port_id_t port_id = (jack_port_id_t) node->data;
	JackPort *port = fGraphManager->GetPort( port_id );

        jack_default_audio_sample_t* buf =
            (jack_default_audio_sample_t*)fGraphManager->GetBuffer(port_id, fEngineControl->fBufferSize);

        const char *portname = port->GetType();

        if (strncmp (portname, JACK_DEFAULT_AUDIO_TYPE, jack_port_type_size()) == 0)
        {
            // audio port, encode celt data.
    
	    int encoded_bytes;
	    float *floatbuf = alloca (sizeof(float) * nframes );
	    memcpy( floatbuf, buf, nframes*sizeof(float) );
	    CELTEncoder *encoder = src_node->data;
	    encoded_bytes = celt_encode_float( encoder, floatbuf, NULL, packet_bufX, net_period_up );
	    if( encoded_bytes != net_period_up )
		printf( "something in celt changed. netjack needs to be changed to handle this.\n" );
	    src_node = jack_slist_next( src_node );
        }
        else if (strncmp(portname, JACK_DEFAULT_MIDI_TYPE, jack_port_type_size()) == 0)
        {
            // encode midi events from port to packet
            // convert the data buffer to a standard format (uint32_t based)
            unsigned int buffer_size_uint32 = net_period_up / 2;
            uint32_t * buffer_uint32 = (uint32_t*) packet_bufX;
            encode_midi_buffer (buffer_uint32, buffer_size_uint32, buf);
        }
        packet_bufX = (packet_bufX + net_period_up);
        node = jack_slist_next (node);
        chn++;
    }
}

#endif
/* Wrapper functions with bitdepth argument... */
void
JackNetOneDriver::render_payload_to_jack_ports (int bitdepth, void *packet_payload, jack_nframes_t net_period_down, JSList *capture_ports, JSList *capture_srcs, jack_nframes_t nframes, int dont_htonl_floats)
{
#if HAVE_CELT
    if (bitdepth == 1000)
        render_payload_to_jack_ports_celt (packet_payload, net_period_down, capture_ports, capture_srcs, nframes);
    else
#endif
        render_payload_to_jack_ports_float (packet_payload, net_period_down, capture_ports, capture_srcs, nframes, dont_htonl_floats);
}

void
JackNetOneDriver::render_jack_ports_to_payload (int bitdepth, JSList *playback_ports, JSList *playback_srcs, jack_nframes_t nframes, void *packet_payload, jack_nframes_t net_period_up, int dont_htonl_floats)
{
#if HAVE_CELT
    if (bitdepth == 1000)
        render_jack_ports_to_payload_celt (playback_ports, playback_srcs, nframes, packet_payload, net_period_up);
    else
#endif
        render_jack_ports_to_payload_float (playback_ports, playback_srcs, nframes, packet_payload, net_period_up, dont_htonl_floats);
}



//driver loader-----------------------------------------------------------------------

#ifdef __cplusplus
    extern "C"
    {
#endif
        SERVER_EXPORT jack_driver_desc_t* driver_get_descriptor ()
        {
            jack_driver_desc_t* desc = ( jack_driver_desc_t* ) calloc ( 1, sizeof ( jack_driver_desc_t ) );

            strcpy ( desc->name, "netone" );                             // size MUST be less then JACK_DRIVER_NAME_MAX + 1
            strcpy ( desc->desc, "netjack one slave backend component" ); // size MUST be less then JACK_DRIVER_PARAM_DESC + 1

            desc->nparams = 9;
            desc->params = ( jack_driver_param_desc_t* ) calloc ( desc->nparams, sizeof ( jack_driver_param_desc_t ) );

            int i = 0;
            strcpy ( desc->params[i].name, "udp_net_port" );
            desc->params[i].character = 'p';
            desc->params[i].type = JackDriverParamInt;
            desc->params[i].value.i = 19000;
            strcpy ( desc->params[i].short_desc, "UDP port" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "mtu" );
            desc->params[i].character = 'M';
            desc->params[i].type = JackDriverParamInt;
            desc->params[i].value.i = 1500;
            strcpy ( desc->params[i].short_desc, "MTU to the master" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "input_ports" );
            desc->params[i].character = 'C';
            desc->params[i].type = JackDriverParamInt;
            desc->params[i].value.i = 2;
            strcpy ( desc->params[i].short_desc, "Number of audio input ports" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "output_ports" );
            desc->params[i].character = 'P';
            desc->params[i].type = JackDriverParamInt;
            desc->params[i].value.i = 2;
            strcpy ( desc->params[i].short_desc, "Number of audio output ports" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "midi_in_ports" );
            desc->params[i].character = 'i';
            desc->params[i].type = JackDriverParamInt;
            desc->params[i].value.i = 0;
            strcpy ( desc->params[i].short_desc, "Number of midi input ports" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "midi_out_ports" );
            desc->params[i].character = 'o';
            desc->params[i].type = JackDriverParamUInt;
            desc->params[i].value.i = 0;
            strcpy ( desc->params[i].short_desc, "Number of midi output ports" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "client_name" );
            desc->params[i].character = 'n';
            desc->params[i].type = JackDriverParamString;
            strcpy ( desc->params[i].value.str, "'hostname'" );
            strcpy ( desc->params[i].short_desc, "Name of the jack client" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "transport_sync" );
            desc->params[i].character  = 't';
            desc->params[i].type = JackDriverParamUInt;
            desc->params[i].value.ui = 1U;
            strcpy ( desc->params[i].short_desc, "Sync transport with master's" );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            i++;
            strcpy ( desc->params[i].name, "mode" );
            desc->params[i].character  = 'm';
            desc->params[i].type = JackDriverParamString;
            strcpy ( desc->params[i].value.str, "normal" );
            strcpy ( desc->params[i].short_desc, "Slow, Normal or Fast mode." );
            strcpy ( desc->params[i].long_desc, desc->params[i].short_desc );

            return desc;
        }

        SERVER_EXPORT Jack::JackDriverClientInterface* driver_initialize ( Jack::JackLockedEngine* engine, Jack::JackSynchro* table, const JSList* params )
        {
            char multicast_ip[16];
            char net_name[JACK_CLIENT_NAME_SIZE];
            int udp_port = 3000;
            int mtu = 1500;
            uint transport_sync = 1;
            jack_nframes_t period_size = 128;
            jack_nframes_t sample_rate = 48000;
            int audio_capture_ports = 2;
            int audio_playback_ports = 2;
            int midi_input_ports = 0;
            int midi_output_ports = 0;
            bool monitor = false;
            char network_mode = 'n';
            const JSList* node;
            const jack_driver_param_t* param;

            net_name[0] = 0;

            for ( node = params; node; node = jack_slist_next ( node ) )
            {
                param = ( const jack_driver_param_t* ) node->data;
                switch ( param->character )
                {
                    case 'a' :
                        strncpy ( multicast_ip, param->value.str, 15 );
                        break;
                    case 'p':
                        udp_port = param->value.ui;
                        break;
                    case 'M':
                        mtu = param->value.i;
                        break;
                    case 'C':
                        audio_capture_ports = param->value.i;
                        break;
                    case 'P':
                        audio_playback_ports = param->value.i;
                        break;
                    case 'i':
                        midi_input_ports = param->value.i;
                        break;
                    case 'o':
                        midi_output_ports = param->value.i;
                        break;
                    case 'n' :
                        strncpy ( net_name, param->value.str, JACK_CLIENT_NAME_SIZE );
                        break;
                    case 't' :
                        transport_sync = param->value.ui;
                        break;
                    case 'm' :
                        if ( strcmp ( param->value.str, "normal" ) == 0 )
                            network_mode = 'n';
                        else if ( strcmp ( param->value.str, "slow" ) == 0 )
                            network_mode = 's';
                        else if ( strcmp ( param->value.str, "fast" ) == 0 )
                            network_mode = 'f';
                        else
                            jack_error ( "Unknown network mode, using 'normal' mode." );
                        break;
                }
            }

            try
            {

                Jack::JackDriverClientInterface* driver =
                    new Jack::JackWaitThreadedDriver (
                    new Jack::JackNetOneDriver ( "system", "net_pcm", engine, table, multicast_ip, udp_port, mtu,
                                              midi_input_ports, midi_output_ports, audio_capture_ports, audio_playback_ports,
					      net_name, transport_sync, network_mode ) );
                if ( driver->Open ( period_size, sample_rate, 1, 1, audio_capture_ports, audio_playback_ports,
                                    monitor, "from_master_", "to_master_", 0, 0 ) == 0 )
                {
                    return driver;
                }
                else
                {
                    delete driver;
                    return NULL;
                }

            }
            catch ( ... )
            {
                return NULL;
            }
        }

#ifdef __cplusplus
    }
#endif
}
