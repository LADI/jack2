/*
Copyright (C) 2001 Paul Davis
Copyright (C) 2004-2008 Grame

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

#include <iostream>
#include <assert.h>
#include <cassert>
#include <csignal>
#include <sys/types.h>
#include <getopt.h>
#include <cstring>
#include <cstdio>

#include "types.h"
#include "jack.h"
#include "JackConstants.h"
#include "JackDriverLoader.h"

#if defined(JACK_DBUS) && defined(__linux__)
#include <dbus/dbus.h> 
#include "audio_reserve.h"
#endif

/*
This is a simple port of the old jackdmp.cpp file to use the new Jack 2.0 control API. Available options for the server
are "hard-coded" in the source. A much better approach would be to use the control API to:
- dynamically retrieve available server parameters and then prepare to parse them
- get available drivers and their possible parameters, then prepare to parse them.
*/

#ifdef __APPLE__
#include <CoreFoundation/CFNotificationCenter.h>
#include <CoreFoundation/CoreFoundation.h>

static void notify_server_start(const char* server_name)
{
    // Send notification to be used in the JackRouter plugin
    CFStringRef ref = CFStringCreateWithCString(NULL, server_name, kCFStringEncodingMacRoman);
    CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterGetDistributedCenter(),
            CFSTR("com.grame.jackserver.start"),
            ref,
            NULL,
            kCFNotificationDeliverImmediately | kCFNotificationPostToAllSessions);
    CFRelease(ref);
}

static void notify_server_stop(const char* server_name)
{
    // Send notification to be used in the JackRouter plugin
    CFStringRef ref1 = CFStringCreateWithCString(NULL, server_name, kCFStringEncodingMacRoman);
    CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterGetDistributedCenter(),
            CFSTR("com.grame.jackserver.stop"),
            ref1,
            NULL,
            kCFNotificationDeliverImmediately | kCFNotificationPostToAllSessions);
    CFRelease(ref1);
}

#else

static void notify_server_start(const char* server_name)
{}
static void notify_server_stop(const char* server_name)
{}

#endif

static void copyright(FILE* file)
{
    fprintf(file, "jackdmp " VERSION "\n"
            "Copyright 2001-2005 Paul Davis and others.\n"
            "Copyright 2004-2010 Grame.\n"
            "jackdmp comes with ABSOLUTELY NO WARRANTY\n"
            "This is free software, and you are welcome to redistribute it\n"
            "under certain conditions; see the file COPYING for details\n");
}

static void usage(FILE* file)
{
    fprintf(file, "\n"
            "usage: jackdmp [ --no-realtime OR -r ]\n"
            "               [ --realtime OR -R [ --realtime-priority OR -P priority ] ]\n"
            "      (the two previous arguments are mutually exclusive. The default is --realtime)\n"
            "               [ --name OR -n server-name ]\n"
            "               [ --timeout OR -t client-timeout-in-msecs ]\n"
            "               [ --loopback OR -L loopback-port-number ]\n"
            "               [ --port-max OR -p maximum-number-of-ports]\n"
            "               [ --midi OR -X midi-driver ]\n"
            "               [ --verbose OR -v ]\n"
#ifdef __linux__
            "               [ --clocksource OR -c [ c(ycle) | h(pet) | s(ystem) ]\n"
#endif
            "               [ --replace-registry ]\n"
            "               [ --silent OR -s ]\n"
            "               [ --sync OR -S ]\n"
            "               [ --temporary OR -T ]\n"
            "               [ --version OR -V ]\n"
            "         -d backend [ ... backend args ... ]\n"
#ifdef __APPLE__
            "               Available backends may include: coreaudio, dummy or net.\n\n"
#endif 
#ifdef WIN32
            "               Available backends may include: portaudio, dummy or net.\n\n"
#endif 
#ifdef __linux__
            "               Available backends may include: alsa, dummy, freebob, firewire or net\n\n"
#endif
#if defined(__sun__) || defined(sun)
            "               Available backends may include: boomer, oss, dummy or net.\n\n"
#endif
            "       jackdmp -d backend --help\n"
            "             to display options for each backend\n\n");
}

// To put in the control.h interface??
static jackctl_driver_t *
jackctl_server_get_driver(
    jackctl_server_t *server,
    const char *driver_name)
{
    const JSList * node_ptr;

    node_ptr = jackctl_server_get_drivers_list(server);

    while (node_ptr)
    {
        if (strcmp(jackctl_driver_get_name((jackctl_driver_t *)node_ptr->data), driver_name) == 0)
        {
            return (jackctl_driver_t *)node_ptr->data;
        }

        node_ptr = jack_slist_next(node_ptr);
    }

    return NULL;
}

static jackctl_parameter_t *
jackctl_get_parameter(
    const JSList * parameters_list,
    const char * parameter_name)
{
    while (parameters_list)
    {
        if (strcmp(jackctl_parameter_get_name((jackctl_parameter_t *)parameters_list->data), parameter_name) == 0)
        {
            return (jackctl_parameter_t *)parameters_list->data;
        }

        parameters_list = jack_slist_next(parameters_list);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    jackctl_server_t * server_ctl;
    const JSList * server_parameters;
    const char* server_name = "default";
    jackctl_driver_t * audio_driver_ctl;
    jackctl_driver_t * midi_driver_ctl;
    jackctl_driver_t * loopback_driver_ctl;
    int replace_registry = 0;
    
    const char *options = "-d:X:P:uvshVrRL:STFl:t:mn:p:"
        "a:"
#ifdef __linux__
        "c:"
#endif
        ;
    
    struct option long_options[] = {
#ifdef __linux__
                                       { "clock-source", 1, 0, 'c' },
#endif
                                       { "loopback-driver", 1, 0, 'L' },
                                       { "audio-driver", 1, 0, 'd' },
                                       { "midi-driver", 1, 0, 'X' },
                                       { "verbose", 0, 0, 'v' },
                                       { "help", 0, 0, 'h' },
                                       { "port-max", 1, 0, 'p' },
                                       { "no-mlock", 0, 0, 'm' },
                                       { "name", 0, 0, 'n' },
                                       { "unlock", 0, 0, 'u' },
                                       { "realtime", 0, 0, 'R' },
                                       { "no-realtime", 0, 0, 'r' }, 
                                       { "replace-registry", 0, &replace_registry, 0 },
                                       { "loopback", 0, 0, 'L' },
                                       { "realtime-priority", 1, 0, 'P' },
                                       { "timeout", 1, 0, 't' },
                                       { "temporary", 0, 0, 'T' },
                                       { "version", 0, 0, 'V' },
                                       { "silent", 0, 0, 's' },
                                       { "sync", 0, 0, 'S' },
                                       { "autoconnect", 1, 0, 'a' },
                                       { 0, 0, 0, 0 }
                                   };

    int i,opt = 0;
    int option_index = 0;
    bool seen_audio_driver = false;
    bool seen_midi_driver = false;
    char *audio_driver_name = NULL;
    char **audio_driver_args = NULL;
    int audio_driver_nargs = 1;
    char *midi_driver_name = NULL;
    char **midi_driver_args = NULL;
    int midi_driver_nargs = 1;
    int do_mlock = 1;
    int do_unlock = 0;
    int loopback = 0;
    bool show_version = false;
    sigset_t signals;
    jackctl_parameter_t* param;
    union jackctl_parameter_value value;

    copyright(stdout);
#if defined(JACK_DBUS) && defined(__linux__)
    server_ctl = jackctl_server_create(audio_acquire, audio_release);
#else
    server_ctl = jackctl_server_create(NULL, NULL);
#endif
    if (server_ctl == NULL) {
        fprintf(stderr, "Failed to create server object\n");
        return -1;
    }
  
    server_parameters = jackctl_server_get_parameters(server_ctl);
    
    // Default setting
    param = jackctl_get_parameter(server_parameters, "realtime");
    if (param != NULL) {
        value.b = true;
        jackctl_parameter_set_value(param, &value);
    }
    
    opterr = 0;
    while (!seen_audio_driver &&
            (opt = getopt_long(argc, argv, options,
                               long_options, &option_index)) != EOF) {
        switch (opt) {

        #ifdef __linux__        
            case 'c':
                param = jackctl_get_parameter(server_parameters, "clock-source");
                if (param != NULL) {
                    if (tolower (optarg[0]) == 'h') {
                        value.ui = JACK_TIMER_HPET;
                        jackctl_parameter_set_value(param, &value);
                    } else if (tolower (optarg[0]) == 'c') {
                        value.ui = JACK_TIMER_CYCLE_COUNTER;
                        jackctl_parameter_set_value(param, &value);
                    } else if (tolower (optarg[0]) == 's') {
                        value.ui = JACK_TIMER_SYSTEM_CLOCK;
                        jackctl_parameter_set_value(param, &value);
                    } else {
                        usage(stdout);
                        goto fail_free1;
                    }
                }
                break;
        #endif

            case 'a':
                param = jackctl_get_parameter(server_parameters, "self-connect-mode");
                if (param != NULL) {
                    bool value_valid = false;
                    for (int k=0; k<jackctl_parameter_get_enum_constraints_count( param ); k++ ) {
                        value = jackctl_parameter_get_enum_constraint_value( param, k );
                        if( value.c == optarg[0] )
                            value_valid = true;
                    }

                    if( value_valid ) {
                        value.c = optarg[0];
                        jackctl_parameter_set_value(param, &value);
                    } else {
                        usage(stdout);
                        goto fail_free1;
                    }
                }
                break;

            case 'd':
                seen_audio_driver = true;
                audio_driver_name = optarg;
                break;
                
            case 'L':
                loopback = atoi(optarg);
                break;

            case 'X':
                seen_midi_driver = true;
                midi_driver_name = optarg;
                break;

            case 'p':
                param = jackctl_get_parameter(server_parameters, "port-max");
                if (param != NULL) {
                    value.ui = atoi(optarg);
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'm':
                do_mlock = 0;
                break;

            case 'u':
                do_unlock = 1;
                break;

            case 'v':
                param = jackctl_get_parameter(server_parameters, "verbose");
                if (param != NULL) {
                    value.b = true;
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 's':
                jack_set_error_function(silent_jack_error_callback);
                break;

            case 'S':
                param = jackctl_get_parameter(server_parameters, "sync");
                if (param != NULL) {
                    value.b = true;
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'n':
                server_name = optarg;
                param = jackctl_get_parameter(server_parameters, "name");
                if (param != NULL) {
                    strncpy(value.str, optarg, JACK_PARAM_STRING_MAX);
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'P':
                param = jackctl_get_parameter(server_parameters, "realtime-priority");
                if (param != NULL) {
                    value.i = atoi(optarg);
                    jackctl_parameter_set_value(param, &value);
                }
                break;
          
            case 'r':
                param = jackctl_get_parameter(server_parameters, "realtime");
                if (param != NULL) {
                    value.b = false;
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'R':
                param = jackctl_get_parameter(server_parameters, "realtime");
                if (param != NULL) {
                    value.b = true;
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'T':
                param = jackctl_get_parameter(server_parameters, "temporary");
                if (param != NULL) {
                    value.b = true;
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 't':
                param = jackctl_get_parameter(server_parameters, "client-timeout");
                if (param != NULL) {
                    value.i = atoi(optarg);
                    jackctl_parameter_set_value(param, &value);
                }
                break;

            case 'V':
                show_version = true;
                break;

            default:
                fprintf(stderr, "unknown option character %c\n", optopt);
                /*fallthru*/

            case 'h':
                usage(stdout);
                goto fail_free1;
        }
    }
    
    // Long option with no letter so treated separately
    param = jackctl_get_parameter(server_parameters, "replace-registry");
    if (param != NULL) {
        value.b = replace_registry;
        jackctl_parameter_set_value(param, &value);
    }
 
    if (show_version) {
        printf( "jackdmp version " VERSION
                " tmpdir " jack_server_dir
                " protocol %d"
                "\n", JACK_PROTOCOL_VERSION);
        return -1;
    }

    if (!seen_audio_driver) {
        usage(stderr);
        goto fail_free1;
    }

    // Audio driver
    audio_driver_ctl = jackctl_server_get_driver(server_ctl, audio_driver_name);
    if (audio_driver_ctl == NULL) {
        fprintf(stderr, "Unknown driver \"%s\"\n", audio_driver_name);
        goto fail_free1;
    }

    if (optind < argc) {
        audio_driver_nargs = 1 + argc - optind;
    } else {
        audio_driver_nargs = 1;
    }

    if (audio_driver_nargs == 0) {
        fprintf(stderr, "No driver specified ... hmm. JACK won't do"
                " anything when run like this.\n");
        goto fail_free1;
    }

    audio_driver_args = (char **) malloc(sizeof(char *) * audio_driver_nargs);
    audio_driver_args[0] = audio_driver_name;

    for (i = 1; i < audio_driver_nargs; i++) {
        audio_driver_args[i] = argv[optind++];
    }

    if (jackctl_parse_driver_params(audio_driver_ctl, audio_driver_nargs, audio_driver_args)) {
        goto fail_free1;
    }

    // Start server
    if (!jackctl_server_start(server_ctl, audio_driver_ctl)) {
        fprintf(stderr, "Failed to start server\n");
        goto fail_free1;
    }

    // MIDI driver
    if (seen_midi_driver) {

        midi_driver_ctl = jackctl_server_get_driver(server_ctl, midi_driver_name);
        if (midi_driver_ctl == NULL) {
            fprintf(stderr, "Unknown driver \"%s\"\n", midi_driver_name);
            goto fail_free2;
        }

        jackctl_server_add_slave(server_ctl, midi_driver_ctl);
    }
    
    // Loopback driver
    if (loopback > 0) {
        loopback_driver_ctl = jackctl_server_get_driver(server_ctl, "loopback");
        if (loopback_driver_ctl != NULL) {
            const JSList * loopback_parameters = jackctl_driver_get_parameters(loopback_driver_ctl);
            param = jackctl_get_parameter(loopback_parameters, "channels");
            if (param != NULL) {
                value.ui = loopback;
                jackctl_parameter_set_value(param, &value);
            }
            jackctl_server_add_slave(server_ctl, loopback_driver_ctl);
        }
    }

    notify_server_start(server_name);

    // Waits for signal
    signals = jackctl_setup_signals(0);
    jackctl_wait_signals(signals);

    if (!jackctl_server_stop(server_ctl))
        fprintf(stderr, "Cannot stop server...\n");
    
    jackctl_server_destroy(server_ctl);
    notify_server_stop(server_name);
    return 0;

fail_free1:
    jackctl_server_destroy(server_ctl);
    return -1;
    
fail_free2:
    jackctl_server_stop(server_ctl);
    jackctl_server_destroy(server_ctl);
    notify_server_stop(server_name);
    return -1;
}
