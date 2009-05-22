/*
Copyright (C) 2008 Grame

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

#include "JackResampler.h"
#include <stdio.h>

namespace Jack
{

JackResampler::JackResampler()
    :fRatio(1),fRingBufferSize(DEFAULT_RB_SIZE)
{
    fRingBuffer = jack_adapterpipe_create(sizeof(float) * fRingBufferSize);
    //jack_adapterpipe_write_advance(fRingBuffer, (sizeof(float) * fRingBufferSize) / 2 - (sizeof(float) * fPeriodSize/2) ) ;
}

JackResampler::~JackResampler()
{
    if (fRingBuffer)
        jack_adapterpipe_free(fRingBuffer);
}

void JackResampler::Reset(unsigned int new_size)
{
    fRingBufferSize = new_size;
    //jack_adapterpipe_reset_size(fRingBuffer, sizeof(float) * fRingBufferSize);
    jack_adapterpipe_reset(fRingBuffer);
    //jack_adapterpipe_write_advance(fRingBuffer, (sizeof(float) * fRingBufferSize / 2));
    //jack_adapterpipe_write_advance(fRingBuffer, (sizeof(float) * fRingBufferSize) / 2 - (sizeof(float) * fPeriodSize/2) ) ;
}

unsigned int JackResampler::ReadSpace()
{
    return (jack_adapterpipe_read_space(fRingBuffer) / sizeof(float));
}

unsigned int JackResampler::WriteSpace()
{
    return (jack_adapterpipe_write_space(fRingBuffer) / sizeof(float));
}

void JackResampler::HardAdjustWrite( int adjust )
{
    jack_adapterpipe_set_write_space(fRingBuffer, sizeof(float) * adjust ) ;
}

void JackResampler::HardAdjustRead( int adjust )
{
    jack_adapterpipe_set_read_space( fRingBuffer, sizeof(float) * adjust ) ;
}

unsigned int JackResampler::Read(float* buffer, unsigned int frames)
{
    jack_adapterpipe_read_no_fail(fRingBuffer, (char*)buffer, frames * sizeof(float));
    return frames;
}

unsigned int JackResampler::Write(float* buffer, unsigned int frames)
{
    jack_adapterpipe_write_no_fail(fRingBuffer, (char*)buffer, frames * sizeof(float));
    return frames;
}

unsigned int JackResampler::ReadResample(float* buffer, unsigned int frames)
{
    printf( "wtf...... :(\n" );
    return Read(buffer, frames);
}

unsigned int JackResampler::WriteResample(float* buffer, unsigned int frames)
{
    printf( "wtf...... :(\n" );
    return Write(buffer, frames);
}

}
