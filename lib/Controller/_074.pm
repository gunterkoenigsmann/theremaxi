
package ThereMaxi::Controller::_074;

use strict;
use warnings;

use base 'ThereMaxi::Controller::prozent';


sub value_import
{
	my($self,$value) = @_;
	$self->SUPER::value_import( ( ( ( $value - 15 ) / 12500 ) ** (1/3) ) * 100 );
}


1;
