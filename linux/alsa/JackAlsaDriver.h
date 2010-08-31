/*
Copyright (C) 2001 Paul Davis
Copyright (C) 2004 Grame

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

#ifndef __JackAlsaDriver__
#define __JackAlsaDriver__

#include "JackAudioDriver.h"
#include "JackThreadedDriver.h"
#include "JackTime.h"

#include "alsa_driver.h"

namespace Jack
{

/*!
\brief The ALSA driver.
*/

class JackAlsaDriver : public JackAudioDriver
{

    private:

        jack_driver_t* fDriver;
        int fReservedCaptureDevice;
        int fReservedPlaybackDevice;
    
        void alsa_driver_release_channel_dependent_memory(alsa_driver_t *driver);
        int alsa_driver_check_capabilities(alsa_driver_t *driver);
        int alsa_driver_check_card_type(alsa_driver_t *driver);
        int alsa_driver_hammerfall_hardware(alsa_driver_t *driver);
        int alsa_driver_hdsp_hardware(alsa_driver_t *driver);
        int alsa_driver_ice1712_hardware(alsa_driver_t *driver);
        int alsa_driver_usx2y_hardware(alsa_driver_t *driver);
        int alsa_driver_generic_hardware(alsa_driver_t *driver);
        int alsa_driver_hw_specific(alsa_driver_t *driver, int hw_monitoring,
                                     int hw_metering);
        int alsa_driver_setup_io_function_pointers (alsa_driver_t *driver);
        int alsa_driver_configure_stream(alsa_driver_t *driver, char *device_name,
                                          const char *stream_name,
                                          snd_pcm_t *handle,
                                          snd_pcm_hw_params_t *hw_params,
                                          snd_pcm_sw_params_t *sw_params,
                                          unsigned int *nperiodsp,
                                          unsigned long *nchns,
                                          unsigned long sample_width);

        int alsa_driver_set_parameters(alsa_driver_t *driver,
                                        jack_nframes_t frames_per_cycle,
                                        jack_nframes_t user_nperiods,
                                        jack_nframes_t rate);

        int	alsa_driver_reset_parameters(alsa_driver_t *driver,
                                          jack_nframes_t frames_per_cycle,
                                          jack_nframes_t user_nperiods,
                                          jack_nframes_t rate);

        int alsa_driver_get_channel_addresses(alsa_driver_t *driver,
                                               snd_pcm_uframes_t *capture_avail,
                                               snd_pcm_uframes_t *playback_avail,
                                               snd_pcm_uframes_t *capture_offset,
                                               snd_pcm_uframes_t *playback_offset);

        jack_driver_t * alsa_driver_new(const char *name, char *playback_alsa_device,
                                        char *capture_alsa_device,
                                        jack_client_t *client,
                                        jack_nframes_t frames_per_cycle,
                                        jack_nframes_t user_nperiods,
                                        jack_nframes_t rate,
                                        int hw_monitoring,
                                        int hw_metering,
                                        int capturing,
                                        int playing,
                                        DitherAlgorithm dither,
                                        int soft_mode,
                                        int monitor,
                                        int user_capture_nchnls,
                                        int user_playback_nchnls,
                                        int shorts_first,
                                        jack_nframes_t capture_latency,
                                        jack_nframes_t playback_latency,
                                        alsa_midi_t *midi);

        void alsa_driver_delete(alsa_driver_t *driver);
        int alsa_driver_start(alsa_driver_t *driver);
        int alsa_driver_stop(alsa_driver_t *driver);
        int alsa_driver_read(alsa_driver_t *driver, jack_nframes_t nframes);
        int alsa_driver_write(alsa_driver_t *driver, jack_nframes_t nframes);

        jack_nframes_t alsa_driver_wait(alsa_driver_t *driver, int extra_fd, int *status, float
                                         *delayed_usecs);

        void alsa_driver_silence_untouched_channels(alsa_driver_t *driver,
                jack_nframes_t nframes);

        int alsa_driver_restart(alsa_driver_t *driver);
        int alsa_driver_xrun_recovery(alsa_driver_t *driver, float *delayed_usecs);
        void jack_driver_init(jack_driver_t *driver);
        void jack_driver_nt_init(jack_driver_nt_t * driver);

    public:

        JackAlsaDriver(const char* name, const char* alias, JackLockedEngine* engine, JackSynchro* table)
		: JackAudioDriver(name, alias, engine, table)
		,fDriver(NULL)
		,fReservedCaptureDevice(-1)
		,fReservedPlaybackDevice(-1)
        {}
        virtual ~JackAlsaDriver()
        {}

        int Open(jack_nframes_t buffer_size,
                 jack_nframes_t user_nperiods,
                 jack_nframes_t samplerate,
                 bool hw_monitoring,
                 bool hw_metering,
                 bool capturing,
                 bool playing,
                 DitherAlgorithm dither,
                 bool soft_mode,
                 bool monitor,
                 int inchannels,
                 int outchannels,
                 bool shorts_first,
                 const char* capture_driver_name,
                 const char* playback_driver_name,
                 jack_nframes_t capture_latency,
                 jack_nframes_t playback_latency,
                 const char* midi_driver_name);

        int Close();
        int Attach();
        int Detach();

        int Start();
        int Stop();

        int Read();
        int Write();

        // BufferSize can be changed
        bool IsFixedBufferSize()
        {
            return false;
        }

        int SetBufferSize(jack_nframes_t buffer_size);

        // jack api emulation for the midi driver
        int is_realtime() const;
        int create_thread(pthread_t *thread, int prio, int rt, void *(*start_func)(void*), void *arg);

        jack_port_id_t port_register(const char *port_name, const char *port_type, unsigned long flags, unsigned long buffer_size);
        int port_unregister(jack_port_id_t port_index);
        void* port_get_buffer(int port, jack_nframes_t nframes);
        int port_set_alias(int port, const char* name);

        jack_nframes_t get_sample_rate() const;
        jack_nframes_t frame_time() const;
        jack_nframes_t last_frame_time() const;
};

} // end of namespace

#endif
