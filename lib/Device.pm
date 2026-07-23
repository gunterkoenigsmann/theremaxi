
package ThereMaxi::Device;

use strict;
use warnings;

use MIDI::ALSA ':CONSTS';

my $MIDI;
my $PEER;
my $NAME;
my $CHAN = 0;


sub name { $NAME || 'not discovered' }


my @DISCO =
(
	SND_SEQ_EVENT_CLIENT_START,
	SND_SEQ_EVENT_PORT_EXIT,
	SND_SEQ_EVENT_PORT_UNSUBSCRIBED,
);

sub init
{
	return ThereMaxi::Editor->status("MIDI: $!") unless MIDI::ALSA::client($main::NAME,2,1,0);

	$MIDI = MIDI::ALSA::id;

	ThereMaxi::Event->connect('prefs/discovery_interval',\&discover);
	ThereMaxi::Event->fire('prefs/discovery_interval',$main::STATE{device}->{discovery_interval});

	return ThereMaxi::Editor->status("MIDI: $!") unless MIDI::ALSA::connectfrom(0,'System:1');

	Glib::IO->add_watch(MIDI::ALSA::fd,[qw( in pri )],sub
	{
		if ( MIDI::ALSA::inputpending )
		{
			my @e = MIDI::ALSA::input;
			&discover if $main::STATE{device}->{discovery_interval} && grep {$e[0]==$_} @DISCO;
			&_CC_($e[7][0],$e[7][4],$e[7][5]) if $PEER && $e[5][0] == $PEER && $e[6][0] == $MIDI && $e[0] == SND_SEQ_EVENT_CONTROLLER;
		}
		1;
	});
}


my $DISCOVER;
sub discover
{
	my %clients = reverse MIDI::ALSA::listclients;
	my $peer = ( grep /theremini/i, keys %clients )[0];
	if ( $PEER && !$peer )
	{
		&midi_close;
		ThereMaxi::Event->fire('device/discover',$DISCOVER=0) if $DISCOVER;
	}
	elsif ( $peer && !$PEER )
	{
		$NAME = $peer;
		$PEER = $clients{$peer};
		&midi_open;
		ThereMaxi::Event->fire('device/discover',$DISCOVER=1) unless $DISCOVER;
	}
	elsif ( ref $_[0] )
	{
		ThereMaxi::Event->fire('device/discover',$DISCOVER);
	}
}


my $SYNC;
sub sync_on_change
{
	my $sync = $SYNC;
	ThereMaxi::Event->fire('device/sync_on_change',$SYNC=( $DISCOVER ? pop : 0 ));
	$sync;
}


sub offline
{
	&midi_close;
	ThereMaxi::Event->fire('device/sync_on_change',$SYNC=0) if $SYNC;
	ThereMaxi::Event->fire('device/discover',$DISCOVER=0) if $DISCOVER;
}


sub info
{
	my $info = &name;
	return $info unless $DISCOVER;
	eval
	{
		local $SIG{__DIE__};
		&midi_sx(qw( 7E 7F 06 01 ));
		my $read = &midi_read;
		if ( $read =~ /^\xf0\x7e\x7f\x06\x02(.*)\xf7$/s )
		{
			$info .= sprintf ' (%d.%d.%d)', unpack 'C3', substr $1, -3;
		}
	};
	my $ret = $@;
	&error($ret);
	$info;
}


sub value_changed
{
	return 0 unless $DISCOVER && $SYNC;
	eval
	{
		local $SIG{__DIE__};
		&midi_send($_[0]->{CC},$_[0]->value_export);
	};
	&error($@);
}


sub select_preset
{
	return 0 unless $DISCOVER && $SYNC;
	my(undef,$nr) = @_;
	eval
	{
		local $SIG{__DIE__};
		&midi_ps($nr);
	};
	my $ret = $@;
	&error($ret);
}


sub load_presets
{
	return 0 unless $DISCOVER;
	my $sync = &sync_on_change(0);
	ThereMaxi::Editor->status('Loading Presets from '.&name);
	ThereMaxi::Event->catch;
	eval
	{
		local $SIG{__DIE__};
		&midi_sx(qw( 04 0B 06 03 00 00 00 00 00 00 00 00 00 00 00 ));
		ThereMaxi::Library->save('.',ThereMaxi::Preset->sysex(&midi_read));
		ThereMaxi::Editor->status('Presets successfully loaded');
	};
	my $ret = $@;
	$main::STATE{preset} = './0';
	ThereMaxi::Event->release;
	&sync_on_change($sync);
	&error($ret);
}


sub send_preset
{
	return 0 unless $DISCOVER;
	return 0 if $SYNC;
	&_send_(0,ThereMaxi::Preset->export_values);
}


sub save_preset
{
	return 0 unless $DISCOVER;
	my(undef,$CC) = @_;
	my %values = ThereMaxi::Preset->export_values;
	%values = ( $CC => $values{$CC} ) if defined $CC;
	my $ok = &_send_(1,%values);
	$main::STATE{preset} = $main::STATE{online} if $ok >= 0;
	$ok;
}


sub _send_
{
	my($save,%values) = @_;
	my $sync = &sync_on_change(0);
	ThereMaxi::Event->catch;
	eval
	{
		local $SIG{__DIE__};
		while ( my($CC,$value) = each %values )
		{
			&midi_send($CC,$value);
		}
		&midi_cc(119,1) if $save;
	};
	my $ret = $@;
	ThereMaxi::Event->release;
	&sync_on_change($sync);
	&error($ret);
}


sub midi_open
{
	return unless $PEER;
	eval
	{
		local $SIG{__DIE__};
		MIDI::ALSA::connectfrom(1,$PEER) or die $!;
		MIDI::ALSA::connectto(2,$PEER) or die $!;
	};
	&error($@);
}


sub midi_close
{
	return unless $PEER;
	eval
	{
		local $SIG{__DIE__};
		MIDI::ALSA::disconnectfrom(1,$PEER);
		MIDI::ALSA::disconnectto(2,$PEER);
	};
	$PEER = undef;
}


sub midi_send
{
	my($CC,$value) = @_;
	if ( $CC =~ /^\d+$/ )
	{
		&midi_cc($CC,$value);
	}
	elsif ( $CC eq '_ps' )
	{
		&midi_sx(qw( 04 0B 06 07 00 00 00 00 00 00 00 00 00 00 00 01 ),@$value,qw( 20 00 ));
	}
	elsif ( $CC eq '_fx' )
	{
		&midi_sx(qw( 04 0B 06 08 00 00 00 00 00 00 00 00 00 00 00 01 ),@$value,qw( 20 00 ));
	}
}


sub midi_cc
{
	my($CC,$value) = @_;
	if ( ref $value )
	{
		MIDI::ALSA::output(MIDI::ALSA::controllerevent($CHAN,$CC,$value->[0]));
		MIDI::ALSA::output(MIDI::ALSA::controllerevent($CHAN,$CC+32,$value->[1]));
	}
	else
	{
		MIDI::ALSA::output(MIDI::ALSA::controllerevent($CHAN,$CC,$value));
	}
}


sub midi_ps
{
	MIDI::ALSA::output(MIDI::ALSA::controllerevent($CHAN,0,0));
	MIDI::ALSA::output(MIDI::ALSA::pgmchangeevent($CHAN,@_));
}


sub midi_sx
{
	MIDI::ALSA::output(MIDI::ALSA::sysex($CHAN,pack('(H2)*',@_)));
}


sub midi_read
{
	my $data = '';
	local $SIG{ALRM} = sub { alarm(0); die "Timeout\n" };
	alarm($main::STATE{device}->{midi_read_timeout});
	while ( $data !~ /\xf7$/ )
	{
		next unless MIDI::ALSA::inputpending;
		my @e = MIDI::ALSA::input;
		$data .= $e[7][0] if $e[0] == SND_SEQ_EVENT_SYSEX
			&& $e[5][0] == $PEER
			&& $e[6][0] == $MIDI
		;
	}
	alarm(0);
	$data;
}


sub error
{
	return 1 unless$_[0];
	ThereMaxi::Event->fire('ERROR',@_);
	-1;
}


my %CC = (volume=>{},pitch=>{});
sub CC
{
	my(undef,$id,$type,$callback) = @_;
	'CODE' eq ref $callback
		? $CC{$type}->{$id} = $callback
		: delete $CC{$type}->{$id}
	;
}


my %value = (volume=>undef,pitch=>undef);
sub _CC_
{
	my($chan,$cc,$value) = @_;
	while ( my($type,$cb) = each %CC )
	{
		next unless $chan == $main::STATE{device}->{midi_input}->{$type}->[0];
		if ( $cc == $main::STATE{device}->{midi_input}->{$type}->[1] )
		{
			if ( $main::STATE{device}->{midi_input}->{$type}->[2] )
			{
				$value{$type} = $value;
			}
			else
			{
				$value{$type} = undef;
				&$_($type,$value) for values %$cb;
			}
		}
		elsif ( ( $cc == $main::STATE{device}->{midi_input}->{$type}->[1] + 32 ) && defined $value{$type} )
		{
			&$_($type,($value{$type}<<7)|$value) for values %$cb;
			$value{$type} = undef;
		}
	}
}


1;
