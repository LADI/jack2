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
	netjack_read( &netj, netj.period_size );

        return 0;
    }

    int JackNetOneDriver::Write()
    {
	netjack_write( &netj, netj.period_size, 0 );
        return 0;
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
