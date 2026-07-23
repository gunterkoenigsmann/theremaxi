
package ThereMaxi::Controller::name;

use strict;
use warnings;

use base 'ThereMaxi::Controller';


sub value_compare
{
	my($self,$a,$b) = @_;
	return 1 unless defined $a;
	return 1 unless defined $b;
	$a ||= '';
	$b ||= '';
	$a cmp $b;
}


sub value_import
{
	my($self,@value) = @_;
	my $value = join '', @value;
	$value =~ s/\s+$//g;
	$value =~ s/^\s+//g;
	$self->SUPER::value_import(substr $value, 0, 13);
}


sub value_export
{
	my($self,$value) = @_;
	$value = $self->{VALUE} unless defined $value;
	$value =~ s/\s+$//g;
	$value =~ s/^\s+//g;
	$value = substr $value, 0, 13;
	while ( length $value < 13 )
	{
		$value .= ' ';
	}
	[ unpack('H*',$value) =~ /../g ];
}


1;
