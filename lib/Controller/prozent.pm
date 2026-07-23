
package ThereMaxi::Controller::prozent;

use strict;
use warnings;

use base 'ThereMaxi::Controller::numeric';


sub define
{
	shift->SUPER::define(min=>0,max=>100,dig=>0,fmt=>'%d %%',@_);
}


1;
