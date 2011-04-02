/*
Copyright (C) 2011 Devin Anderson

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

#include <cassert>
#include <memory>
#include <stdexcept>

#include "JackError.h"
#include "JackMidiUtil.h"
#include "JackWinMMEInputPort.h"
#include "JackMidiWriteQueue.h"

using Jack::JackWinMMEInputPort;

///////////////////////////////////////////////////////////////////////////////
// Static callbacks
///////////////////////////////////////////////////////////////////////////////

void CALLBACK
JackWinMMEInputPort::HandleMidiInputEvent(HMIDIIN handle, UINT message,
                                          DWORD port, DWORD param1,
                                          DWORD param2)
{
    ((JackWinMMEInputPort *) port)->ProcessWinMME(message, param1, param2);
}

///////////////////////////////////////////////////////////////////////////////
// Class
///////////////////////////////////////////////////////////////////////////////

JackWinMMEInputPort::JackWinMMEInputPort(const char *alias_name,
                                         const char *client_name,
                                         const char *driver_name, UINT index,
                                         size_t max_bytes, size_t max_messages)
{
    thread_queue = new JackMidiAsyncQueue(max_bytes, max_messages);
    std::auto_ptr<JackMidiAsyncQueue> thread_queue_ptr(thread_queue);
    write_queue = new JackMidiBufferWriteQueue();
    std::auto_ptr<JackMidiBufferWriteQueue> write_queue_ptr(write_queue);
    sysex_buffer = new jack_midi_data_t[max_bytes];
    char error_message[MAXERRORLENGTH];
    MMRESULT result = midiInOpen(&handle, index, (DWORD)HandleMidiInputEvent,
                                 (DWORD)this,
                                 CALLBACK_FUNCTION | MIDI_IO_STATUS);
    if (result != MMSYSERR_NOERROR) {
        GetInErrorString(result, error_message);
        goto delete_sysex_buffer;
    }
    sysex_header.dwBufferLength = max_bytes;
    sysex_header.dwBytesRecorded = 0;
    sysex_header.dwFlags = 0;
    sysex_header.dwUser = 0;
    sysex_header.lpData = (LPSTR)(((LPBYTE) &sysex_header) + sizeof(MIDIHDR));
    sysex_header.lpNext = 0;
    result = midiInPrepareHeader(handle, &sysex_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        GetInErrorString(result, error_message);
        goto close_handle;
    }
    result = midiInAddBuffer(handle, &sysex_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        GetInErrorString(result, error_message);
        goto unprepare_header;
    }

    MIDIINCAPS capabilities;
    char *name_tmp;
    result = midiInGetDevCaps(index, &capabilities, sizeof(capabilities));
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [constructor]", "midiInGetDevCaps",
                   result);
        name_tmp = (char*) driver_name;
    } else {
        name_tmp = capabilities.szPname;
    }

    snprintf(alias, sizeof(alias) - 1, "%s:%s:in%d", alias_name, name_tmp,
             index + 1);
    snprintf(name, sizeof(name) - 1, "%s:capture_%d", client_name, index + 1);
    jack_event = 0;
    started = false;
    write_queue_ptr.release();
    thread_queue_ptr.release();
    return;

 unprepare_header:
    result = midiInUnprepareHeader(handle, &sysex_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [constructor]",
                   "midiInUnprepareHeader", result);
    }
 close_handle:
    result = midiInClose(handle);
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [constructor]", "midiInClose", result);
    }
 delete_sysex_buffer:
    delete[] sysex_buffer;
    throw std::runtime_error(error_message);
}

JackWinMMEInputPort::~JackWinMMEInputPort()
{
    Stop();
    MMRESULT result = midiInReset(handle);
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [destructor]", "midiInReset", result);
    }
    result = midiInUnprepareHeader(handle, &sysex_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [destructor]", "midiInUnprepareHeader",
                   result);
    }
    result = midiInClose(handle);
    if (result != MMSYSERR_NOERROR) {
        WriteInError("JackWinMMEInputPort [destructor]", "midiInClose", result);
    }
    delete[] sysex_buffer;
    delete thread_queue;
    delete write_queue;
}

void
JackWinMMEInputPort::EnqueueMessage(jack_nframes_t time, size_t length,
                                    jack_midi_data_t *data)
{
    switch (thread_queue->EnqueueEvent(time, length, data)) {
    case JackMidiWriteQueue::BUFFER_FULL:
        jack_error("JackWinMMEInputPort::EnqueueMessage - The thread queue "
                   "cannot currently accept a %d-byte event.  Dropping event.",
                   length);
        break;
    case JackMidiWriteQueue::BUFFER_TOO_SMALL:
        jack_error("JackWinMMEInputPort::EnqueueMessage - The thread queue "
                   "buffer is too small to enqueue a %d-byte event.  Dropping "
                   "event.", length);
        break;
    default:
        ;
    }
}

void
JackWinMMEInputPort::ProcessJack(JackMidiBuffer *port_buffer,
                                 jack_nframes_t frames)
{
    write_queue->ResetMidiBuffer(port_buffer, frames);
    if (! jack_event) {
        jack_event = thread_queue->DequeueEvent();
    }
    for (; jack_event; jack_event = thread_queue->DequeueEvent()) {
        switch (write_queue->EnqueueEvent(jack_event)) {
        case JackMidiWriteQueue::BUFFER_TOO_SMALL:
            jack_error("JackWinMMEMidiInputPort::Process - The buffer write "
                       "queue couldn't enqueue a %d-byte event. Dropping "
                       "event.", jack_event->size);
            // Fallthrough on purpose
        case JackMidiWriteQueue::OK:
            continue;
        }
        break;
    }
}

void
JackWinMMEInputPort::ProcessWinMME(UINT message, DWORD param1, DWORD param2)
{
    set_threaded_log_function();
    jack_nframes_t current_frame = GetCurrentFrame();
    switch (message) {
    case MIM_CLOSE:
        jack_info("JackWinMMEInputPort::ProcessWinMME - MIDI device closed.");
        break;
    case MIM_MOREDATA:
        jack_info("JackWinMMEInputPort::ProcessWinMME - The MIDI input device "
                  "driver thinks that JACK is not processing messages fast "
                  "enough.");
        // Fallthrough on purpose.
    case MIM_DATA:
        jack_midi_data_t message_buffer[3];
        jack_midi_data_t status = param1 & 0xff;
        int length = GetMessageLength(status);
        switch (length) {
        case 3:
            message_buffer[2] = param1 & 0xff0000;
            // Fallthrough on purpose.
        case 2:
            message_buffer[1] = param1 & 0xff00;
            // Fallthrough on purpose.
        case 1:
            message_buffer[0] = status;
            break;
        case 0:
            jack_error("JackWinMMEInputPort::ProcessWinMME - **BUG** MIDI "
                       "input driver sent an MIM_DATA message with a sysex "
                       "status byte.");
            return;
        case -1:
            jack_error("JackWinMMEInputPort::ProcessWinMME - **BUG** MIDI "
                       "input driver sent an MIM_DATA message with an invalid "
                       "status byte.");
            return;
        }
        EnqueueMessage(current_frame, (size_t) length, message_buffer);
        break;
    case MIM_LONGDATA:
        LPMIDIHDR header = (LPMIDIHDR) param1;
        jack_midi_data_t *data = (jack_midi_data_t *) header->lpData;
        size_t length1 = header->dwBytesRecorded;
        if ((data[0] != 0xf0) || (data[length1 - 1] != 0xf7)) {
            jack_error("JackWinMMEInputPort::ProcessWinMME - Discarding "
                       "%d-byte sysex chunk.", length);
        } else {
            EnqueueMessage(current_frame, length1, data);
        }
        // Is this realtime-safe?  This function isn't run in the JACK thread,
        // but we still want it to perform as quickly as possible.  Even if
        // this isn't realtime safe, it may not be avoidable.
        MMRESULT result = midiInAddBuffer(handle, &sysex_header,
                                          sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR) {
            WriteInError("JackWinMMEInputPort::ProcessWinMME", "midiInAddBuffer",
                       result);
        }
        break;
    case MIM_LONGERROR:
        jack_error("JackWinMMEInputPort::ProcessWinMME - Invalid or "
                   "incomplete sysex message received.");
        break;
    case MIM_OPEN:
        jack_info("JackWinMMEInputPort::ProcessWinMME - MIDI device opened.");
    }
}

bool
JackWinMMEInputPort::Start()
{
    if (! started) {
        MMRESULT result = midiInStart(handle);
        started = result == MMSYSERR_NOERROR;
        if (! started) {
            WriteInError("JackWinMMEInputPort::Start", "midiInStart", result);
        }
    }
    return started;
}

bool
JackWinMMEInputPort::Stop()
{
    if (started) {
        MMRESULT result = midiInStop(handle);
        started = result != MMSYSERR_NOERROR;
        if (started) {
            WriteInError("JackWinMMEInputPort::Stop", "midiInStop", result);
        }
    }
    return ! started;
}

void
JackWinMMEInputPort::GetInErrorString(MMRESULT error, LPTSTR text)
{
    MMRESULT result = midiInGetErrorText(error, text, MAXERRORLENGTH);
    if (result != MMSYSERR_NOERROR) {
        snprintf(text, MAXERRORLENGTH, "Unknown error code '%d'", error);
    }
}

void
JackWinMMEInputPort::WriteInError(const char *jack_func, const char *mm_func,
                                MMRESULT result)
{
    char error_message[MAXERRORLENGTH];
    GetInErrorString(result, error_message);
    jack_error("%s - %s: %s", jack_func, mm_func, error_message);
}

