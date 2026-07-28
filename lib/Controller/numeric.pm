
package ThereMaxi::Controller::numeric;

use strict;
use warnings;

use base 'ThereMaxi::Controller';


sub define
{
	shift->SUPER::define(min=>0,dig=>0,fmt=>'%d',@_);
}


sub range_lower { sprintf $_[0]->{fmt}, $_[0]->{min} }
sub range_upper { sprintf $_[0]->{fmt}, $_[0]->{max} }


sub widget
{
	my($self) = @_;
	die 'MIN' unless defined $self->{min};
	die 'MAX' unless defined $self->{max};
	die 'DIG' unless defined $self->{dig};
	die 'FMT' unless defined $self->{fmt};

	my $abs = abs($self->{min}) + abs($self->{max});

	$self->{step} ||= ( $self->{CC} < 32 && $abs > 0x7f ) ? 1 : $abs/0x7f;

	my $slider = $self->{__WIDGET__} = Gtk2::HScale->new_with_range($self->{min},$self->{max},$self->{step});
	$slider->set_digits($self->{dig});
	$slider->add_mark($self->{min},'bottom',$self->range_lower);
	$slider->add_mark( $self->{min} < 0 ? 0 : $abs/2 ,'bottom',undef);
	$slider->add_mark($self->{max},'bottom',$self->range_upper);
	$slider->set_draw_value(1);
	$slider->set_value_pos('top');
	$slider->signal_connect(format_value=>sub{ sprintf $self->{fmt}, $_[1] });
	$slider->signal_connect(value_changed=>sub{ $self->value_changed($_[0]->get_value) });
	$self->SUPER::widget($slider,sub{ $slider->set_value($_[0]||0) });
}


sub value_import
{
	my($self,$value) = @_;
	# NaN compares false against everything, so it would slip past the range
	# check below and end up in the library file, where it is invalid JSON.
	$value = $self->{min} unless defined($value) && $value == $value;
	$value = sprintf '%.'.$self->{dig}.'f', $value;
	$value =~ tr/,/./; # no locale does'nt work !?
	$value = $self->{min} if $value < $self->{min};
	$value = $self->{max} if $value > $self->{max};
	$self->SUPER::value_import($value);
}


sub value_export
{
	my($self,$value) = @_;
	$value = $self->{VALUE} unless defined $value;

	my $abs  = abs($self->{min}) + abs($self->{max});
	my $wide = ( $self->{CC} < 32 && $abs > 0x7f );
	my $wire_max = $wide ? 0x3fff : 0x7f;

	my $wire;

	# Unsigned 14-bit parameters go on the wire in their storage units - the
	# value times its divisor - not scaled to the display maximum. For most that
	# is the same number, but the delay time's range on the wire (about 1000 ms)
	# is wider than the 836 ms shown, so only the divisor scaling reads back
	# correctly from the device.
	my $div = ( $wide && $self->{min} >= 0 )
		? $ThereMaxi::Preset::WIRE_DIVISOR{$self->{CC}} : undef;
	if ( defined $div )
	{
		return 0 if $value <= $self->{min};
		$wire = int( $value * $div + 0.5 );
	}
	else
	{
		# Scale [min, max] onto the wire: subtract the minimum so it sits at 0 -
		# for a floored parameter like the scan rate that is not zero - and round
		# to nearest, which truncating did not, reading back a step low. This also
		# carries the maximum and the signed centre correctly at full 14-bit width.
		my $span = $self->{max} - $self->{min};
		$wire = int( ( $value - $self->{min} ) * $wire_max / $span + 0.5 );
	}

	$wire = $wire_max if $wire > $wire_max;
	$wire = 0         if $wire < 0;

	return [ $wire >> 7, $wire & 0x7f ] if $wide;
	$wire & 0x7f;
}


sub get_value
{
	my($self) = @_;
	my $value = $self->{VALUE};
	$value = $self->{min} unless defined $value;
	$value =~ tr/,/./;
	$value;
}


1;
