
package ThereMaxi::Controller::_090dep;

use strict;
use warnings;

use base 'ThereMaxi::Controller::numeric';


my @enabled;

sub depend
{
	my($self,$event,$CC,$value) = @_;
	return unless $CC eq '90';
	@enabled = ThereMaxi::Controller::_090->enabled unless @enabled;
	$value ||= 0;
	$self->{__WIDGET__}->set_sensitive( $value > $#enabled ? 1 : $enabled[$value] );
}


1;
