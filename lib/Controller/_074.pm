
package ThereMaxi::Controller::_074;

use strict;
use warnings;

use base 'ThereMaxi::Controller::prozent';


sub value_import
{
	my($self,$value) = @_;
	# ** cannot raise a negative number to a fractional power, so a cutoff word
	# below 15 used to give NaN. That travelled through the clamping in
	# numeric::value_import untouched - NaN compares false against everything -
	# into the library file, where it is invalid JSON, and the library could
	# never be loaded again. Take the cube root by sign instead; the parent
	# clamps the negative result to the minimum.
	my $x = ( $value - 15 ) / 12500;
	my $root = $x < 0 ? -( (-$x) ** (1/3) ) : $x ** (1/3);
	$self->SUPER::value_import( $root * 100 );
}


1;
