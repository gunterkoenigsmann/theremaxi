
package ThereMaxi::Event;

use strict;
use warnings;


my %EVENTS = map { $_ => [] } qw
(
	ERROR
	device/discover
	device/sync_on_change
	controller/set_value
	controller/value_changed
	sync/library
	sync/presets
	switch/library
	switch/presets
	preset/unload
	preset/loaded
	preset/changed
	prefs/discovery_interval
	prefs/midi_input
	feature/MidiFeedbackLoop
);
my @EVENTS;
my $EVENTS = 'fire';


sub connect
{
	my(undef,%connect) = @_;
	while ( my($event,$callback) = each %connect )
	{
		die "Unknown Event '$event'" unless exists $EVENTS{$event};
		die 'CODEREF expected' unless 'CODE' eq ref $callback;
		push @{$EVENTS{$event}}, $callback;
	}
}


sub fire
{
	my(undef,$event,@values) = @_;
	die "Unknown Event '$event'" unless exists $EVENTS{$event};
	if ( $EVENTS eq 'fire' )
	{
		&$_($event,@values) for @{$EVENTS{$event}};
	}
	elsif ( $EVENTS eq 'queue' )
	{
		push @EVENTS, [$event=>@values];
	}
	else
	{
		die "EVENTS=$EVENTS";
	}
}


sub catch
{
	$EVENTS = 'queue';
}


sub release
{
	$EVENTS = 'fire';
	while ( @EVENTS )
	{
		&fire(undef,@{ shift @EVENTS });
	}
}


1;
