/*
	SuperCollider real time audio synthesis system
	Copyright (c) 2002 James McCartney. All rights reserved.
	http://www.audiosynth.com

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
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "SC_CoreAudio.h"
#include <stdarg.h>
#include "SC_Prototypes.h"
#include "SC_HiddenWorld.h"
#include "SC_WorldOptions.h"
#include "SC_Time.hpp"
#include <math.h>
#include <stdlib.h>

#include "BeagleRT.h"

int32 server_timeseed()
{
	return timeSeed();
}

int64 gOSCoffset = 0;

static inline int64 GetCurrentOSCTime()
{
	return OSCTime(getTime());
}

int64 oscTimeNow()
{
	return GetCurrentOSCTime();
}

void initializeScheduler()
{
	gOSCoffset = GetCurrentOSCTime();
}



class SC_BelaDriver : public SC_AudioDriver
{

	int mInputChannelCount, mOutputChannelCount;
	uint64_t mBelaStartSampleCount; // probably always starts as zero, but we need to subtract it anyway to keep track of total elapsed samples.
protected:
	// Driver interface methods
	virtual bool DriverSetup(int* outNumSamplesPerCallback, double* outSampleRate);
	virtual bool DriverStart();
	virtual bool DriverStop();

public:
	SC_BelaDriver(struct World *inWorld);
	virtual ~SC_BelaDriver();

	void BelaAudioCallback(BeagleRTContext *context);
};

SC_AudioDriver* SC_NewAudioDriver(struct World *inWorld)
{
	return new SC_BelaDriver(inWorld);
}

SC_BelaDriver::SC_BelaDriver(struct World *inWorld)
		: SC_AudioDriver(inWorld)
		, mStartHostSecs(0)
{
}

SC_BelaDriver::~SC_BelaDriver()
{
	// Clean up any resources allocated for audio
	BeagleRT_cleanupAudio();
}

void render(BeagleRTContext *belaContext, void *userData)
//static int SC_PortAudioStreamCallback( const void *input, void *output,
//	unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo,
//	PaStreamCallbackFlags statusFlags, void *userData )
{
	SC_BelaDriver *driver = (SC_BelaDriver*)userData;

	driver->BelaAudioCallback(context);
}
void sc_SetDenormalFlags();
void SC_BelaDriver::BelaAudioCallback(BeagleRTContext *context)
{
	sc_SetDenormalFlags();
	World *world = mWorld;

	// NOTE: code here is adapted from the SC_Jack.cpp, the version not using the DLL
	HostTime hostTime = getTime();
	double hostSecs = secondsSinceEpoch(hostTime);
	// NOTE: jack driver uses time in num frames; bela gives it in num samples, so here we divide to give in num frames. need to do this?
	double sampleTime = static_cast<double>(belaContext->audioSampleCount / belaContext->audioFrames);
	//double sampleTime = (double)(jack_frame_time(client) + jack_frames_since_cycle_start(client));

	if (mStartHostSecs == 0) {
		mStartHostSecs = hostSecs;
		mStartSampleTime = sampleTime;
	} else {
		double instSampleRate = (sampleTime - mPrevSampleTime) / (hostSecs - mPrevHostSecs);
		double smoothSampleRate = mSmoothSampleRate;
		smoothSampleRate = smoothSampleRate + 0.002 * (instSampleRate - smoothSampleRate);
		if (fabs(smoothSampleRate - mSampleRate) > 10.) {
			smoothSampleRate = mSampleRate;
		}
		mOSCincrement = (int64)(mOSCincrementNumerator / smoothSampleRate);
		mSmoothSampleRate = smoothSampleRate;
#if 0
		double avgSampleRate = (sampleTime - mStartSampleTime)/(hostSecs - mStartHostSecs);
		double jitter = (smoothSampleRate * (hostSecs - mPrevHostSecs)) - (sampleTime - mPrevSampleTime);
		double drift = (smoothSampleRate - mSampleRate) * (hostSecs - mStartHostSecs);
#endif
	}

	mPrevHostSecs = hostSecs;
	mPrevSampleTime = sampleTime;

	try {
		mFromEngine.Free();
		mToEngine.Perform();
		mOscPacketsToEngine.Perform();

		int numInputs = mInputList->mSize;
		int numOutputs = mOutputList->mSize;
		jack_port_t **inPorts = mInputList->mPorts;
		jack_port_t **outPorts = mOutputList->mPorts;
		sc_jack_sample_t **inBuffers = mInputList->mBuffers;
		sc_jack_sample_t **outBuffers = mOutputList->mBuffers;

		int numSamples = NumSamplesPerCallback();
		int bufFrames = mWorld->mBufLength;
		int numBufs = numSamples / bufFrames;

		float *inBuses = mWorld->mAudioBus + mWorld->mNumOutputs * bufFrames;
		float *outBuses = mWorld->mAudioBus;
		int32 *inTouched = mWorld->mAudioBusTouched + mWorld->mNumOutputs;
		int32 *outTouched = mWorld->mAudioBusTouched;

		int minInputs = sc_min(numInputs, (int)mWorld->mNumInputs);
		int minOutputs = sc_min(numOutputs, (int)mWorld->mNumOutputs);

		int bufFramePos = 0;

// TODO
		// cache I/O buffers
		for (int i = 0; i < minInputs; ++i) {
			inBuffers[i] = (sc_jack_sample_t*)jack_port_get_buffer(inPorts[i], numSamples);
		}

		for (int i = 0; i < minOutputs; ++i) {
			outBuffers[i] = (sc_jack_sample_t*)jack_port_get_buffer(outPorts[i], numSamples);
		}

		// main loop
#ifdef SC_JACK_USE_DLL
		int64 oscTime = mOSCbuftime = (int64)((mDLL.PeriodTime() - mMaxOutputLatency) * kSecondsToOSCunits + .5);
// 		int64 oscInc = mOSCincrement = (int64)(mOSCincrementNumerator / mDLL.SampleRate());
		int64 oscInc = mOSCincrement = (int64)((mDLL.Period() / numBufs) * kSecondsToOSCunits + .5);
		mSmoothSampleRate = mDLL.SampleRate();
		double oscToSamples = mOSCtoSamples = mSmoothSampleRate * kOSCtoSecs /* 1/pow(2,32) */;
#else
		int64 oscTime = mOSCbuftime = OSCTime(hostTime) - (int64)(mMaxOutputLatency * kSecondsToOSCunits + .5);
		int64 oscInc = mOSCincrement;
		double oscToSamples = mOSCtoSamples;
#endif

		for (int i = 0; i < numBufs; ++i, mWorld->mBufCounter++, bufFramePos += bufFrames) {
			int32 bufCounter = mWorld->mBufCounter;
			int32 *tch;

//TODO
			// copy+touch inputs
			tch = inTouched;
			for (int k = 0; k < minInputs; ++k) {
				sc_jack_sample_t *src = inBuffers[k] + bufFramePos;
				float *dst = inBuses + k * bufFrames;
				for (int n = 0; n < bufFrames; ++n) {
					*dst++ = *src++;
				}
				*tch++ = bufCounter;
			}

			// run engine
			int64 schedTime;
			int64 nextTime = oscTime + oscInc;

			while ((schedTime = mScheduler.NextTime()) <= nextTime) {
				float diffTime = (float)(schedTime - oscTime) * oscToSamples + 0.5;
				float diffTimeFloor = floor(diffTime);
				world->mSampleOffset = (int)diffTimeFloor;
				world->mSubsampleOffset = diffTime - diffTimeFloor;

				if (world->mSampleOffset < 0) world->mSampleOffset = 0;
				else if (world->mSampleOffset >= world->mBufLength) world->mSampleOffset = world->mBufLength-1;

				SC_ScheduledEvent event = mScheduler.Remove();
				event.Perform();
			}

			world->mSampleOffset = 0;
			world->mSubsampleOffset = 0.f;
			World_Run(world);

//TODO
			// copy touched outputs
			tch = outTouched;
			for (int k = 0; k < minOutputs; ++k) {
				sc_jack_sample_t *dst = outBuffers[k] + bufFramePos;
				if (*tch++ == bufCounter) {
					float *src = outBuses + k * bufFrames;
					for (int n = 0; n < bufFrames; ++n) {
						*dst++ = *src++;
					}
				} else {
					for (int n = 0; n < bufFrames; ++n) {
						*dst++ = 0.0f;
					}
				}
			}

			// advance OSC time
			mOSCbuftime = oscTime = nextTime;
		}
	} catch (std::exception& exc) {
		scprintf("SC_BelaDriver: exception in real time: %s\n", exc.what());
	} catch (...) {
		scprintf("SC_BelaDriver: unknown exception in real time\n");
	}

	mAudioSync.Signal();
}

// ====================================================================
//
//
bool SC_BelaDriver::DriverSetup(int* outNumSamples, double* outSampleRate)
{
	BeagleRTInitSettings settings;
	BeagleRT_defaultSettings(&settings);	// This function should be called in main() before parsing any command-line arguments. It
				// sets default values in the data structure which specifies the BeagleRT settings, including
				// frame sizes, numbers of channels, volume levels and other parameters.



	/*
	TODO DAN NOTE: we do NOT know the samplerate or blocksize in here. anything that DOES need to know that goes in the setup() callback.
	*/


	//TODO


	int mDeviceInOut[2];
	const PaDeviceInfo *pdi;
	const PaHostApiInfo *apiInfo;
	const PaStreamInfo *psi;
	PaTime suggestedLatencyIn, suggestedLatencyOut;
	PaDeviceIndex numDevices = Pa_GetDeviceCount();

	mDeviceInOut[0] = paNoDevice;
	mDeviceInOut[1] = paNoDevice;
	if (mDeviceInOut[0] == paNoDevice) mDeviceInOut[0] = Pa_GetDefaultInputDevice();
	if (mDeviceInOut[1] == paNoDevice) mDeviceInOut[1] = Pa_GetDefaultOutputDevice();

	*outNumSamples = mWorld->mBufLength;
	if (mPreferredSampleRate)
		*outSampleRate = mPreferredSampleRate;
	else
		*outSampleRate = 44100.;


//TODO
	if (mPreferredHardwareBufferFrameSize)
		// controls the suggested latency by hardwareBufferSize switch -Z
		suggestedLatencyIn = suggestedLatencyOut = mPreferredHardwareBufferFrameSize / (*outSampleRate);
	else {
		if (mDeviceInOut[0]!=paNoDevice)
			suggestedLatencyIn = Pa_GetDeviceInfo( mDeviceInOut[0] )->defaultLowInputLatency;
		if (mDeviceInOut[1]!=paNoDevice)
			suggestedLatencyOut = Pa_GetDeviceInfo( mDeviceInOut[1] )->defaultLowOutputLatency;
	}

	fprintf(stdout, "\nBooting with:\n");

//TODO
	PaSampleFormat fmt = paFloat32 | paNonInterleaved;
//TODO
	if (mDeviceInOut[0]!=paNoDevice){
		mInputChannelCount = Pa_GetDeviceInfo( mDeviceInOut[0] )->maxInputChannels;
		fprintf(stdout, "  In: %s : %s \n",
		Pa_GetHostApiInfo(Pa_GetDeviceInfo( mDeviceInOut[0] )->hostApi)->name,
		Pa_GetDeviceInfo( mDeviceInOut[0] )->name);
	}else{
		mInputChannelCount = 0;
	}
//TODO
	if (mDeviceInOut[1]!=paNoDevice){
		mOutputChannelCount = Pa_GetDeviceInfo( mDeviceInOut[1] )->maxOutputChannels;
		fprintf(stdout, "  Out: %s : %s \n",
		Pa_GetHostApiInfo(Pa_GetDeviceInfo( mDeviceInOut[1] )->hostApi)->name,
		Pa_GetDeviceInfo( mDeviceInOut[1] )->name);
	}else{
		mOutputChannelCount = 0;
	}

//TODO
	PaStreamParameters *inStreamParams_p;
	PaStreamParameters inStreamParams;
	if (mDeviceInOut[0]!=paNoDevice){
		inStreamParams.device = mDeviceInOut[0];
		inStreamParams.channelCount = mInputChannelCount;
		inStreamParams.sampleFormat = fmt;
		inStreamParams.suggestedLatency = suggestedLatencyIn;
		inStreamParams.hostApiSpecificStreamInfo = NULL;
		inStreamParams_p = &inStreamParams;
	}else{
		inStreamParams_p = NULL;
	}

//TODO
	PaStreamParameters *outStreamParams_p;
	PaStreamParameters outStreamParams;
	if (mDeviceInOut[1]!=paNoDevice){
		outStreamParams.device = mDeviceInOut[1];
		outStreamParams.channelCount = mOutputChannelCount;
		outStreamParams.sampleFormat = fmt;
		outStreamParams.suggestedLatency = suggestedLatencyOut;
		outStreamParams.hostApiSpecificStreamInfo = NULL;
		outStreamParams_p = &outStreamParams;
	}else{
		outStreamParams_p = NULL;
	}

//TODO
	//Pa_OpenStream(&mStream, inStreamParams_p, outStreamParams_p, *outSampleRate, *outNumSamples, paNoFlag, SC_PortAudioStreamCallback, this );
	// Initialise the PRU audio device. This function prepares audio rendering in BeagleRT. It should be called from main() sometime
	// after command line option parsing has finished. It will initialise the rendering system, which
	// in the process will result in a call to the user-defined setup() function.
	if(BeagleRT_initAudio(&settings, &driver) != 0) {
		cout << "Error in SC_BelaDriver::DriverSetup(): unable to initialise audio" << endl;
		return false;
	}

	return true;
}

// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.
bool setup(BeagleRTContext *context, void *userData)
{
	if(userData != 0){
		//TODO ----- at this point we know the period size and sample rate that Bela has chosen. do we now need to do anything to tell SC?
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().
void cleanup(BeagleRTContext *context, void *userData)
{
}

bool SC_BelaDriver::DriverStart()
{
	// Start the audio device running
	if(BeagleRT_startAudio()) {
		cout << "Error in SC_BelaDriver::DriverStart(): unable to start real-time audio" << endl;
		return false;
	}
	return true;
}

bool SC_BelaDriver::DriverStop()
{
	// Stop the audio device
	BeagleRT_stopAudio();
	return true;
}
