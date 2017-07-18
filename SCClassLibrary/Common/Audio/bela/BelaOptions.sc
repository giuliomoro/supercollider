BelaOptions{

	var <>dacLevel;
	var <>adcLevel;
	var <>pgaGainLeft;
	var <>pgaGainRight;
	var <>headphoneLevel;
	var <>speakerMuted;

	var <>numAnalogChannels;
	var <>numDigitalChannels;

	var <>numMultiplexChannels;
	var <>belaPRU;

	*new{
		^super.new;
	}

	asOptionsString{
		var o = "\"";
		// --dac-level [-D] dBs:               Set the DAC output level (0dB max; -63.5dB min)
		if (dacLevel.notNil, {
			o = o ++ " -D " ++ dacLevel;
		});
		// --adc-level [-A] dBs:               Set the ADC input level (0dB max; -12dB min)
		if (adcLevel.notNil, {
			o = o ++ " -A " ++ adcLevel;
		});
		// --pga-gain-left dBs:                Set the Programmable Gain Amplifier for the left audio channel (0dBmin; 59.5dB max; default: 16dB)
		if (pgaGainLeft.notNil, {
			o = o ++ " --pga-gain-left " ++ pgaGainLeft;
		});
		// --pga-gain-right dBs:               Set the Programmable Gain Amplifier for the right audio channel (0dBmin; 59.5dB max; default: 16dB)
		if (pgaGainRight.notNil, {
			o = o ++ " -pga-gain-right " ++ pgaGainRight;
		});
		// --hp-level [-H] dBs:                Set the headphone output level (0dB max; -63.5dB min)
		if (headphoneLevel.notNil, {
			o = o ++ " -H " ++ headphoneLevel;
		});
		// --mute-speaker [-M] val:            Set whether to mute the speaker initially (default: no)
		if (speakerMuted.notNil, {
			o = o ++ " -M " ++ speakerMuted;
		});
		// --use-analog [-N] val:              Set whether to use ADC/DAC analog (default: yes)
		// --analog-channels [-C] val:         Set the number of ADC/DAC channels (default: 8)
		if (numAnalogChannels.notNil, {
			if ( numAnalogChannels > 0 ){
				o = o ++ " -N " ++ 1;
				o = o ++ " -C " ++ numAnalogChannels;
			}{
				o = o ++ " -N " ++ 0;
			}
		});
		// --use-digital [-G] val:             Set whether to use digital GPIO channels (default: yes)
		// --digital-channels [-B] val:        Set the number of GPIO channels (default: 16)
		if (numDigitalChannels.notNil, {
			if ( numDigitalChannels > 0 ){
				o = o ++ " -G " ++ 1;	// use digital
				o = o ++ " -B " ++ numDigitalChannels;
			}{
				o = o ++ " -G " ++ 0;	// use digital
			}
		});
		// --mux-channels [-X] val:            Set the number of channels to use on the multiplexer capelet (default: not used)
 		if (numMultiplexChannels.notNil, {
			o = o ++ " -X " ++ numMultiplexChannels;
		});
		// --pru-number val:                   Set the PRU to use for I/O (options: 0 or 1, default: 0)
		if (belaPRU.notNil, {
			o = o ++ " -pru-number " ++ belaPRU;
		});
		o = o ++ "\"";
		^o;
	}
}

/*
	--period [-p] period:               Set the hardware period (buffer) size in audio samples = ServerOptions.hardwareBufferSize

	/// not relevant
   --receive-port [-R] val:            Set the receive port (default: 9998)
   --transmit-port [-T] val:           Set the transmit port (default: 9999)
   --server-name [-S] val:             Set the destination server name (default: '127.0.0.1')

	/// set through the interaction between numOutBusChannels / numInputChannels
   --audio-expander-inputs [-Y] vals:  Set the analog inputs to use with audio expander (comma-separated list)
   --audio-expander-outputs [-Z] vals: Set the analog outputs to use with audio expander (comma-separated list)

	// could be useful?
   --pru-file val:                     Set an optional external file to use for the PRU binary code
   --detect-underruns val:             Set whether to warn the user in case of underruns (options: 0 or 1, default: 1)
   --disable-led                       Disable the blinking LED indicator
   --disable-cape-button-monitoring    Disable the monitoring of the Bela cape button (which otherwise stops the running program)
   --uniform-sample-rate               Internally resample the analog channels so that they match the audio sample rate
   --verbose [-v]:                     Enable verbose logging information
*/
