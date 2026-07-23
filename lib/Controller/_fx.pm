
package ThereMaxi::Controller::_fx;

use strict;
use warnings;

use base 'ThereMaxi::Controller::name';


my @choice =
(                   # c12 c14
	[ 'Off'        ,[   0,  0 ]],
	[ 'Short Delay',[ 100, 10 ]],
	[ 'Med Delay'  ,[ 256, 20 ]],
	[ 'Long Delay' ,[ 700, 30 ]],
	# own filters
	[ 'PRESET'                 ], # needs to bee last in list
);


my $depend_change;
my $change_depend;


sub widget
{
	my($self) = @_;

	ThereMaxi::Event->connect
	(
		'preset/unload' => sub{ $change_depend = $depend_change = 0 },
		'preset/loaded' => sub{ $change_depend = $depend_change = 1 },
	);

	my %select = map {$choice[$_]->[0]=>$_} 0 .. $#choice-1;

	my $simple = Gtk2::ComboBox->new_text->get_model; $simple->set($simple->append,0=>$choice[$_]->[0]) for 0 .. $#choice-1;
	my $extend = Gtk2::ComboBox->new_text->get_model; $extend->set($extend->append,0=>$choice[$_]->[0]) for 0 .. $#choice;

	my $combo = $self->{__WIDGET__} = Gtk2::ComboBox->new_text;
	$combo->set_model($simple);

	$combo->signal_connect(changed=>sub
	{
		(my$save,$depend_change) = ($depend_change,0);
		my $value = $combo->get_active;
		if ( $self->value_changed($choice[$value]->[0]) )
		{
			if ( $change_depend && (my$depend=$choice[$value]->[1]) )
			{
				ThereMaxi::Controller->get(12)->set_value($depend->[0]);
				ThereMaxi::Controller->get(14)->set_value($depend->[1]);
			}
			$combo->set_model( $value < $#choice ? $simple : $extend );
			$combo->set_active($value); # re-set iter in model !
		}
		$depend_change = $save;
	});

	$self->{LAYOUT}->{ComboSizeGroup}->add_widget($combo) if $self->{LAYOUT}->{ComboSizeGroup};
	$self->{Alignment} = [0,0.5];
	$self->SUPER::widget($combo,sub
	{
		if ( defined( my $value = defined($_[0]) ? $select{$_[0]} : 0 ) )
		{
			$combo->set_model($simple);
			$combo->set_active($value);
		}
		else
		{
			$combo->set_model($extend);
			$combo->set_active($#choice);
		}
	});
}


sub depend
{
	my($self,$event,$CC,$value) = @_;
	$self->set_value($choice[$#choice]->[0]) if $depend_change && ( ($CC eq '12') || ($CC eq '14') );
}


1;
