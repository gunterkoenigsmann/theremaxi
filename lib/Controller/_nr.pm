
package ThereMaxi::Controller::_nr;

use strict;
use warnings;

use base 'ThereMaxi::Controller';


sub widget
{
	my($self) = @_;
	my $label = Gtk2::Label->new;
	$self->{set_value} = sub{ $label->set_text(sprintf '%02d', $_[0]+1) };
	$label;
}


1;
