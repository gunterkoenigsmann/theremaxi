
package ThereMaxi::Input;

use strict;
use warnings;

# Reassembling an antenna's MIDI input, pulled out of Device.pm's _CC_ so the
# logic is shared with the C port and testable without ALSA. An antenna is
# configured as [ channel, controller, wide ]: in 7-bit mode a value on the
# controller is passed straight through; in 14-bit mode the controller carries
# the high seven bits and controller+32 the low seven, so a value is only
# produced once both have arrived.
#
# feed() is a pure step: given the pending high bits (or undef), the config, and
# an incoming (channel, controller, value), it returns the new pending state and
# the value produced, if any.

sub feed
{
	my($pending,$config,$chan,$cc,$value) = @_;
	my($cfg_chan,$cfg_cc,$wide) = @$config;

	return ($pending,undef) unless $chan == $cfg_chan;

	if ( $cc == $cfg_cc )
	{
		return ($value,undef) if $wide;   # hold the high bits
		return (undef,$value);            # 7-bit: pass through
	}

	if ( $cc == $cfg_cc + 32 && defined $pending )
	{
		return (undef,($pending<<7)|$value);
	}

	return ($pending,undef);
}


1;
