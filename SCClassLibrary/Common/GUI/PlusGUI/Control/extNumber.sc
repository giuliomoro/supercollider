+ SimpleNumber {

	guessDecimalPlaces{ |max=6|
		var dec = 0;
		while( { ( ( this.round( pow(10,-1*dec) ) - this ) < 0 and: ( dec < max ) ) },{ dec = dec + 1; } );
		^dec
	}

}