
package ThereMaxi::Controller::_notes;

use strict;
use warnings;

use base 'ThereMaxi::Controller::_086';


sub range
{
	my($self) = @_;
	my @notes = $self->SUPER::range;
	( map { my $i=$_; map {sprintf '%-2s %d',$_,$i} @notes } 0 .. 10 )[ 0 .. 127 ];
}


1;
