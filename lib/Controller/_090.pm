
package ThereMaxi::Controller::_090;

use strict;
use warnings;

use base 'ThereMaxi::Controller::choice';


my @choice =
(
	[0,'Sine'],
	[0,'Triangle'],
	[0,'Super Saw'],
	[1,'Animoog'],
	[0,'Bright'],
	[0,'Hollow'],
	[1,'Etherwave'],
	[1,'RCA #7'],
	[1,'RCA #8'],
	[1,'RCA #9'],
);

sub range   { map {$_->[1]} @choice }
sub enabled { map {$_->[0]} @choice } # -> _090dep


sub value_import
{
	my($self,@value) = @_;
	my $value = 0;
	$value += $_ for @value;
	$self->SUPER::value_import($value);
}


1;
