
package ThereMaxi::Controller::_086;

use strict;
use warnings;

use base 'ThereMaxi::Controller::choice';


sub range
{(
	'C','C#',
	'D','D#',
	'E',
	'F','F#',
	'G','G#',
	'A','A#',
	'B'
)}


sub range_lower { my @range = $_[0]->range; $range[0]       }
sub range_upper { my @range = $_[0]->range; $range[$#range] }


1;
