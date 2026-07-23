
package ThereMaxi::Controller::choice;

use strict;
use warnings;

use base 'ThereMaxi::Controller';


sub range_lower {'-'}
sub range_upper {'-'}


sub widget
{
	my($self) = @_;
	my $combo = $self->{__WIDGET__} = Gtk2::ComboBox->new_text;
	$combo->append_text($_) for $self->range;
	$combo->signal_connect(changed=>sub{ $self->value_changed($_[0]->get_active) });
	$self->{LAYOUT}->{ComboSizeGroup}->add_widget($combo) if $self->{LAYOUT}->{ComboSizeGroup};
	$self->{Alignment} = [0,0.5];
	$self->SUPER::widget($combo,sub{ $combo->set_active($_[0]||0) });
}


1;
