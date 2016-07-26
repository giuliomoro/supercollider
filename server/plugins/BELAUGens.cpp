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

// ********** this version for windows and linux. for mac see UIUGens.mm

/*
 *  BELA I/O UGens created by nescivi, (c) 2016
 *  https://www.nescivi.eu
 */

// #include <SC_Lock.h>

#include <atomic>

#include "BeagleRT.h"
// Xenomai-specific includes
#include <sys/mman.h>
#include <native/task.h>
#include <native/timer.h>
#include <native/intr.h>
#include <rtdk.h>

#include "SC_PlugIn.h"

static InterfaceTable *ft;


struct AnalogInput : public Unit
{
  int mAudioFramesPerAnalogFrame;
};

struct AnalogOutput : public Unit
{
  int mAudioFramesPerAnalogFrame;
};

// static digital pin, static function (in)
struct DigitalInput : public Unit
{
  int mDigitalPin;
};

// static digital pin, static function (out)
struct DigitalOutput : public Unit
{
  int mDigitalPin;
};

// flexible digital pin, flexible function (in or out)
struct DigitalIO : public Unit
{
  int mLastOutput;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

void AnalogInput_next(AnalogInput *unit, int inNumSamples)
{
  World *world = unit->mWorld;
  int bufLength = world->mBufLength;
  BeagleRTContext *context = world->mBelaContext;

//   rt_printf("INFO: world %p, context %p.\n", world, context );
  
  float *fin = IN(0); // analog in pin, can be modulated
  float *out = ZOUT(0);
  float analogPin = 0;
  float analogValue = 0;
  
  // context->audioFrames should be equal to inNumSamples
//   for(unsigned int n = 0; n < context->audioFrames; n++) {
  for(unsigned int n = 0; n < inNumSamples; n++) {
// 	analogPin = (++*fin);
	analogPin = fin[n];
	rt_printf( "analog pin %f, n %i, inNumSamples %i", analogPin, n, inNumSamples );
	if(!(n % unit->mAudioFramesPerAnalogFrame)) {
	  analogValue = analogReadFrame(context, n/unit->mAudioFramesPerAnalogFrame, (int) analogPin);
	}
	*++out = analogValue;
  }
}

void AnalogInput_Ctor(AnalogInput *unit)
{
	BeagleRTContext *context = unit->mWorld->mBelaContext;
  
	rt_printf("INFO: constructor - belaContext %p.\n", context );
	
	if(context->analogFrames == 0 || context->analogFrames > context->audioFrames) {
		rt_printf("Error: the UGen needs BELA analog enabled, with 4 or 8 channels\n");
		return;
	}

	unit->mAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

	// initiate first sample
	AnalogInput_next( unit, 1);  
	// set calculation method
	SETCALC(AnalogInput_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void AnalogOutput_next(AnalogOutput *unit, int inNumSamples)
{
  World *world = unit->mWorld;
  int bufLength = world->mBufLength;
  BeagleRTContext *context = world->mBelaContext;

  float *fin = IN(0); // analog in pin, can be modulated
  float *in = IN(1);
  
  float analogPin = 0;
  float newinput = 0;
//   float analogValue = 0;
  // context->audioFrames should be equal to inNumSamples
//   for(unsigned int n = 0; n < context->audioFrames; n++) {
  for(unsigned int n = 0; n < inNumSamples; n++) {
	// read input
// 	analogPin = (++*fin);
	analogPin = fin[n];
	newinput = in[n];
// 	newinput = ++*in; // read next input sample
	if(!(n % unit->mAudioFramesPerAnalogFrame)) {
	  analogWriteFrameOnce(context,  n/ unit->mAudioFramesPerAnalogFrame, (int) analogPin, newinput);
	}
  }
}

void AnalogOutput_Ctor(AnalogOutput *unit)
{
	BeagleRTContext *context = unit->mWorld->mBelaContext;
  
	if(context->analogFrames == 0 ) {
		rt_printf("Error: the UGen needs BELA analog enabled\n");
		return;
	}

	unit->mAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

	// initiate first sample
	AnalogOutput_next( unit, 1);  
	// set calculation method
	SETCALC(AnalogOutput_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DigitalInput_next(DigitalInput *unit, int inNumSamples)
{
  World *world = unit->mWorld;
  int bufLength = world->mBufLength;
  BeagleRTContext *context = world->mBelaContext;

  int pinid = unit->mDigitalPin;
  int digitalValue;
  float *out = ZOUT(0);
  
  // context->audioFrames should be equal to inNumSamples
//   for(unsigned int n = 0; n < context->audioFrames; n++) {
  for(unsigned int n = 0; n < inNumSamples; n++) {
// 	if(!(n % unit->mAudioFramesPerAnalogFrame)) {
// 	  analogValue = analogReadFrame(context, n/gAudioFramesPerAnalogFrame, iAnalogIn);
// 	}
	digitalValue=digitalReadFrame(context, n, pinid); //read the value of the button    
	*++out = (float) digitalValue;
  }
}

void DigitalInput_Ctor(DigitalInput *unit)
{
	BeagleRTContext *context = unit->mWorld->mBelaContext;
  
	float fDigitalIn = ZIN0(0); // digital in pin -- cannot change after construction
	unit->mDigitalPin = (int) fDigitalIn;
	pinModeFrame(context, 0, unit->mDigitalPin, INPUT);
	
	// initiate first sample
	DigitalInput_next( unit, 1);  
	// set calculation method
	SETCALC(DigitalInput_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DigitalOutput_next(DigitalOutput *unit, int inNumSamples)
{
  World *world = unit->mWorld;
  int bufLength = world->mBufLength;
  BeagleRTContext *context = world->mBelaContext;

  int pinid = unit->mDigitalPin;
  float *in = IN(1);
  
  float newinput = 0;

  // context->audioFrames should be equal to inNumSamples
//   for(unsigned int n = 0; n < context->audioFrames; n++) {
  for(unsigned int n = 0; n < inNumSamples; n++) {
	// read input
	newinput = ++*in; // read next input sample
	if ( newinput > 0 ){ newinput = 1; }{ newinput = 0; }
	digitalWriteFrameOnce(context, n, pinid, (int) newinput);
  }
}

void DigitalOutput_Ctor(DigitalOutput *unit)
{
	BeagleRTContext *context = unit->mWorld->mBelaContext;

	float fDigital = ZIN0(0); // digital in pin -- cannot change after construction
	unit->mDigitalPin = (int) fDigital;
	pinModeFrame(context, 0, unit->mDigitalPin, OUTPUT);

	// initiate first sample
	DigitalOutput_next( unit, 1);  
	// set calculation method
	SETCALC(DigitalOutput_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////


void DigitalIO_next(DigitalIO *unit, int inNumSamples)
{
  World *world = unit->mWorld;
  int bufLength = world->mBufLength;
  BeagleRTContext *context = world->mBelaContext;

  float *pinid = IN(0);
  float *in = IN(1); // input value
  float *iomode = IN(2); // IO mode : < 0.5 = input, else output
  float *out = ZOUT(0); // output value = last output value
  
  int newpin;
  float newmode = 0; // input
  float newinput = 0;
  int newoutput = unit->mLastOutput;

  // context->audioFrames should be equal to inNumSamples
//   for(unsigned int n = 0; n < context->digitalFrames; n++) {
  for(unsigned int n = 0; n < inNumSamples; n++) {
	// read input
	newpin = (int) ++*pinid; // get pin id
	newinput = ++*in; // read next input sample
	newmode = ++*iomode; // get mode for this pin
	if ( newmode < 0.5 ){
	  pinModeFrameOnce( context, n, newpin, INPUT );
	  newoutput=digitalReadFrame(context, n, newpin);
	} else {	  
	  pinModeFrameOnce( context, n, newpin, OUTPUT );
	  digitalWriteFrameOnce(context, n, newpin, (int) newinput);
	}
	// always write to the output of the UGen
	*++out = (float) newoutput;
  }
  unit->mLastOutput = newoutput;
}

void DigitalIO_Ctor(DigitalIO *unit)
{
	BeagleRTContext *context = unit->mWorld->mBelaContext;

	unit->mLastOutput = 0;

	// initiate first sample
	DigitalIO_next( unit, 1);  
	// set calculation method
	SETCALC(DigitalIO_next);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// extern "C"
// {
// 
//   
// }

// // the functions below are needed??
// 
// void render(BeagleRTContext *belaContext, void *userData)
// {
// // 	SC_BelaDriver *driver = (SC_BelaDriver*)userData;
// // 	driver->BelaAudioCallback(belaContext);
// }
// // setup() is called once before the audio rendering starts.
// // Use it to perform any initialisation and allocation which is dependent
// // on the period size or sample rate.
// //
// // userData holds an opaque pointer to a data structure that was passed
// // in from the call to initAudio().
// //
// // Return true on success; returning false halts the program.
// bool setup(BeagleRTContext* belaContext, void* userData)
// {
// 	if(userData == 0){
// 		printf("BelaPLUGINS: error, setup() got no user data\n");
// 		return false;
// 	}
// 
// 	return true;
// }
// 
// // cleanup() is called once at the end, after the audio has stopped.
// // Release any resources that were allocated in setup().
// void cleanup(BeagleRTContext *belaContext, void *userData)
// {
// }


PluginLoad(BELA)
{
	ft = inTable;

	DefineSimpleUnit(AnalogInput);
	DefineSimpleUnit(AnalogOutput);
	DefineSimpleUnit(DigitalInput);
	DefineSimpleUnit(DigitalOutput);
	DefineSimpleUnit(DigitalIO);
// 	DefineUnit("AnalogInput", sizeof(AnalogInputUGen), (UnitCtorFunc)&AnalogInput_Ctor, 0, 0);
}


// C_LINKAGE SC_API_EXPORT void unload(InterfaceTable *inTable)
// {
// 
// }
