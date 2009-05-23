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

#include "JackAudioAdapter.h"
#include "JackLibSampleRateResampler.h"
#include "JackTime.h"
#include <stdio.h>

namespace Jack
{

#ifdef JACK_MONITOR

    void MeasureTable::Write(int time1, int time2, float r1, float r2, int pos1, int pos2)
    {
        int pos = (++fCount) % TABLE_MAX;
        fTable[pos].time1 = time1;
        fTable[pos].time2 = time2;
        fTable[pos].r1 = r1;
        fTable[pos].r2 = r2;
        fTable[pos].pos1 = pos1;
        fTable[pos].pos2 = pos2;
    }

    void MeasureTable::Save(unsigned int fHostBufferSize, unsigned int fHostSampleRate, unsigned int fAdaptedSampleRate, unsigned int fAdaptedBufferSize)
    {
        char buffer[1024];
        FILE* file = fopen("JackAudioAdapter.log", "w");

        int max = (fCount) % TABLE_MAX - 1;
        for (int i = 1; i < max; i++)
        {
            fprintf(file, "%d \t %d \t %d  \t %f \t %f \t %d \t %d \n",
                    fTable[i].delta, fTable[i].time1, fTable[i].time2,
                    fTable[i].r1, fTable[i].r2, fTable[i].pos1, fTable[i].pos2);
        }
        fclose(file);

        // No used for now
        // Adapter timing 1
        file = fopen("AdapterTiming1.plot", "w");
        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
        fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"frames\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 2 title \"Ringbuffer error\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 3 title \"Ringbuffer error with timing correction\" with lines");
        fprintf(file, buffer);

        fprintf(file, "\n unset multiplot\n");
        fprintf(file, "set output 'AdapterTiming1.pdf\n");
        fprintf(file, "set terminal pdf\n");

        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
        fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"frames\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 2 title \"Consumer interrupt period\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 3 title \"Producer interrupt period\" with lines");
        fprintf(file, buffer);

        fclose(file);

        // Adapter timing 2
        file = fopen("AdapterTiming2.plot", "w");
        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
        fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"resampling ratio\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 4 title \"Ratio 1\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 5 title \"Ratio 2\" with lines");
        fprintf(file, buffer);

        fprintf(file, "\n unset multiplot\n");
        fprintf(file, "set output 'AdapterTiming2.pdf\n");
        fprintf(file, "set terminal pdf\n");

        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
        fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"resampling ratio\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 4 title \"Ratio 1\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 5 title \"Ratio 2\" with lines");
        fprintf(file, buffer);

        fclose(file);

        // Adapter timing 3
        file = fopen("AdapterTiming3.plot", "w");
        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
         fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"frames\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 6 title \"Frames position in consumer ringbuffer\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 7 title \"Frames position in producer ringbuffer\" with lines");
        fprintf(file, buffer);

        fprintf(file, "\n unset multiplot\n");
        fprintf(file, "set output 'AdapterTiming3.pdf\n");
        fprintf(file, "set terminal pdf\n");

        fprintf(file, "set multiplot\n");
        fprintf(file, "set grid\n");
        fprintf(file, "set title \"Audio adapter timing: host [rate = %.1f kHz buffer = %d frames] adapter [rate = %.1f kHz buffer = %d frames] \"\n"
            ,float(fHostSampleRate)/1000.f, fHostBufferSize, float(fAdaptedSampleRate)/1000.f, fAdaptedBufferSize);
        fprintf(file, "set xlabel \"audio cycles\"\n");
        fprintf(file, "set ylabel \"frames\"\n");
        fprintf(file, "plot ");
        sprintf(buffer, "\"JackAudioAdapter.log\" using 6 title \"Frames position in consumer ringbuffer\" with lines,");
        fprintf(file, buffer);
        sprintf(buffer, "\"JackAudioAdapter.log\" using 7 title \"Frames position in producer ringbuffer\" with lines");
        fprintf(file, buffer);

        fclose(file);
    }

#endif

    void JackAudioAdapterInterface::GrowRingBufferSize()
    {
        fRingbufferCurSize *= 2;
    }

    void JackAudioAdapterInterface::AdaptRingBufferSize()
    {
        if (fHostBufferSize > fAdaptedBufferSize)
            fRingbufferCurSize = 4 * fHostBufferSize;
        else
            fRingbufferCurSize = 4 * fAdaptedBufferSize;
    }

    void JackAudioAdapterInterface::ResetRingBuffers()
    {
        if (fRingbufferCurSize > DEFAULT_RB_SIZE)
            fRingbufferCurSize = DEFAULT_RB_SIZE;

        for (int i = 0; i < fCaptureChannels; i++) {
            fCaptureRingBuffer[i]->Reset(fRingbufferCurSize);
	    fCaptureRingBuffer[i]->HardAdjustRead( fRingbufferCurSize/2+fHostBufferSize/2 );
	}
        for (int i = 0; i < fPlaybackChannels; i++) {
            fPlaybackRingBuffer[i]->Reset(fRingbufferCurSize);
	    fPlaybackRingBuffer[i]->HardAdjustWrite( fRingbufferCurSize/2 - fHostBufferSize/2 );
	}
    }

    void JackAudioAdapterInterface::Reset()
    {
        ResetRingBuffers();
        fRunning = false;
    }

    void JackAudioAdapterInterface::Create()
    {
        //ringbuffers
        fCaptureRingBuffer = new JackResampler*[fCaptureChannels];
        fPlaybackRingBuffer = new JackResampler*[fPlaybackChannels];

        if (fAdaptative) {
            AdaptRingBufferSize();
            jack_info("Ringbuffer automatic adaptative mode size = %d frames", fRingbufferCurSize);
        } else {
            if (fRingbufferCurSize > DEFAULT_RB_SIZE)
                fRingbufferCurSize = DEFAULT_RB_SIZE;
            jack_info("Fixed ringbuffer size = %d frames", fRingbufferCurSize);
        }

        for (int i = 0; i < fCaptureChannels; i++ ) {
            fCaptureRingBuffer[i] = new JackLibSampleRateResampler(fQuality, fHostBufferSize);
            fCaptureRingBuffer[i]->Reset(fRingbufferCurSize);
	    fCaptureRingBuffer[i]->HardAdjustRead( fRingbufferCurSize/2 + fHostBufferSize/2 );
        }
        for (int i = 0; i < fPlaybackChannels; i++ ) {
            fPlaybackRingBuffer[i] = new JackLibSampleRateResampler(fQuality, fHostBufferSize);
            fPlaybackRingBuffer[i]->Reset(fRingbufferCurSize);
	    fPlaybackRingBuffer[i]->HardAdjustWrite( fRingbufferCurSize/2 - fHostBufferSize/2 );
        }

        if (fCaptureChannels > 0)
            jack_error("ReadSpace = %ld", fCaptureRingBuffer[0]->ReadSpace());
        if (fPlaybackChannels > 0)
            jack_error("WriteSpace = %ld", fPlaybackRingBuffer[0]->WriteSpace());
    }

    void JackAudioAdapterInterface::Destroy()
    {
        for (int i = 0; i < fCaptureChannels; i++ )
            delete ( fCaptureRingBuffer[i] );
        for (int i = 0; i < fPlaybackChannels; i++ )
            delete ( fPlaybackRingBuffer[i] );

        delete[] fCaptureRingBuffer;
        delete[] fPlaybackRingBuffer;
    }

    int JackAudioAdapterInterface::PullAndPush(float** inputBuffer, float** outputBuffer, unsigned int frames)
    {
        bool failure = false;
        fRunning = true;

	double ratio;

        // Finer estimation of the position in the ringbuffer
        int delta_frames = (fPullAndPushTime > 0) ? (int)((float(long(GetMicroSeconds() - fPullAndPushTime)) * float(fAdaptedSampleRate)) / 1000000.f) : 0;
	//int delta_frames = 0;

	int fill;
	if( fCaptureChannels > 0 ) {
	    fill = fCaptureRingBuffer[0]->ReadSpace();

	    if( (fill < (fHostBufferSize + 10)) || (fill > (fRingbufferCurSize)) || (fCaptureRingBuffer[0]->HasXRun()) ) {
		//jack_error( "capture: buffer bounds reached fill = %d", fill );
		for (int i = 0; i < fCaptureChannels; i++) {
		    fCaptureRingBuffer[i]->HardAdjustRead( fRingbufferCurSize/2 + fHostBufferSize/2 );
		}
		fPIControler_Capture.OutOfBounds();
	    }

	    ratio = fPIControler_Capture.GetRatio( fill - (fRingbufferCurSize/2 + fHostBufferSize/2) + delta_frames);

	    for (int i = 0; i < fCaptureChannels; i++) {
		fCaptureRingBuffer[i]->SetRatio(1.0/ratio);
		if (fCaptureRingBuffer[i]->ReadResample(inputBuffer[i], frames) < frames)
		    failure = true;
	    }

	}

	if( fPlaybackChannels > 0 ) {

	    fill = fPlaybackRingBuffer[0]->ReadSpace();

	    if( (fill < (fAdaptedBufferSize + 2)) || (fill > (fRingbufferCurSize + fAdaptedBufferSize)) || (fPlaybackRingBuffer[0]->HasXRun()) ) {

		//jack_error( "playback buffer bounds reached fill = %d", fill );
		for (int i = 0; i < fPlaybackChannels; i++) {
		    fPlaybackRingBuffer[i]->HardAdjustWrite( fRingbufferCurSize/2 - fHostBufferSize/2 + fAdaptedBufferSize );
		}
		fPIControler_Playback.OutOfBounds();
	    }

	    ratio = fPIControler_Playback.GetRatio( fill - (fRingbufferCurSize/2 - fHostBufferSize/2 + fAdaptedBufferSize)  - delta_frames);

	    for (int i = 0; i < fPlaybackChannels; i++) {
		fPlaybackRingBuffer[i]->SetRatio(ratio);
		if (fPlaybackRingBuffer[i]->WriteResample(outputBuffer[i], frames) < frames)
		    fPlaybackRingBuffer[i]->Reset(fRingbufferCurSize);
	    }
        }
	return 0;
    }

    int JackAudioAdapterInterface::PushAndPull(float** inputBuffer, float** outputBuffer, unsigned int frames)
    {
        if (!fRunning)
            return 0;

        int res = 0;

        // Push/pull from ringbuffer
        for (int i = 0; i < fCaptureChannels; i++) {
            if (fCaptureRingBuffer[i]->Write(inputBuffer[i], frames) < frames)
                res = -1;
        }

        for (int i = 0; i < fPlaybackChannels; i++) {
            if (fPlaybackRingBuffer[i]->Read(outputBuffer[i], frames) < frames)
                res = -1;
        }

        fPullAndPushTime = GetMicroSeconds();
        return res;
    }

} // namespace
