
package ThereMaxi::Preset;

use strict;
use warnings;


my %CONTROLLER;

sub init
{
	%CONTROLLER = map {$_->{CC}=>$_} ThereMaxi::Controller->list_in_preset unless %CONTROLLER;
	ThereMaxi::Library->init;
	ThereMaxi::Device->init;
}


sub load
{
	my(undef,$path) = @_;
	ThereMaxi::Event->fire('preset/unload',split '/', $main::STATE{preset}||'');
	$main::STATE{preset} = $path;
	my @path = split '/', $path;
	my %data = ThereMaxi::Library->load_preset(@path);
	my $sync = ThereMaxi::Device->sync_on_change(0);
	$_->set_value($data{$_->{CC}}) for values %CONTROLLER;
	my $ok = $sync ? ThereMaxi::Device->send_preset : 1;
	ThereMaxi::Device->sync_on_change($sync);
	ThereMaxi::Event->fire('preset/loaded',@path);
	$ok;
}


sub save { save_local(@_) if ThereMaxi::Device::save_preset(@_) >= 0 }

sub save_local
{
	my(undef,$CC) = @_;
	my @path = split '/', $main::STATE{preset};
	return ThereMaxi::Library->save_preset(@path,&get_values) unless defined $CC;
	ThereMaxi::Library->save_preset(@path,$CC=>$CONTROLLER{$CC}->{VALUE});
}


sub changes
{
	my(undef,$CC) = @_;
	return 0 unless $CONTROLLER{$CC};
	my %values = ThereMaxi::Library->load_preset(split '/', $main::STATE{preset});
	%values = ( $CC => $values{$CC} ) if defined $CC;
	my $change = 0;
	while ( my($CC,$value) = each %values )
	{
		$change=1,last if $CONTROLLER{$CC}->value_compare($CONTROLLER{$CC}->{VALUE},$value);
	}
	ThereMaxi::Event->fire('preset/changed',$change);
	$change;
}


sub get_values    { map { $_->{CC} => $_->get_value    } values %CONTROLLER }
sub export_values { map { $_->{CC} => $_->value_export } values %CONTROLLER }


# our, not my: read by tools/dump-protocol.pl - see lib/Controller.pm
our %IMPORT =
(
	0x00 => [ _nr => 'S'              ], # Preset Nr.
	0x02 => [ _ps => '(Zx)13'         ], # Preset Name
#	0x1c .. 0x1f => ?
	0x20 => [  85 => 'S'              ], # Scale
	0x22 => [  86 => 'S'              ], # Root Note
	0x24 => [  84 => 'S', 0x4000/100  ], # Pitch Correction Amount
	0x26 => [ 102 => 's'              ], # Transpose
#	0x28 .. 0x29 => ?
	0x2a => [  12 => 'S', 0x4000/1000 ], # Delay Time
#	0x2c .. 0x2d => ?
	0x2e => [  14 => 'S', 0x4000/100  ], # Delay Feedback
#	0x30 .. 0x33 => ?
	0x34 => [  91 => 'S', 0x4000/100  ], # Effect Mix
	0x36 => [ _fx => 'Z13'            ], # Effect Name
#	0x43 .. 0x45 => padding 0x20 * 3
#	0x46 .. 0x4f => ?
	0x50 => [  90 => 'S'              ], # Wave Selection - part 1
	0x52 => [   9 => 'S', 0x0200      ], # Wavetable Scan Rate
	0x54 => [  90 => 'S'              ], # Wave Selection - part 2
	0x56 => [  74 => 'v'              ], # Filter Cutoff Freq
	0x58 => [  71 => 'S', 0x4000/100  ], # Filter Resonance
	0x5a => [  80 => 'S'              ], # Filter Type
	0x5c => [  29 => 's', 0x0400/100  ], # Filter Pitch Tracking
	0x5e => [  28 => 's', 0x0400/100  ], # Vol Mod Resonance
	0x60 => [  27 => 's', 0x0400/100  ], # Vol Mod Cutoff
	0x62 => [  21 => 'S', 0x4000      ], # Scan Position
	0x64 => [  20 => 'S', 0x4000      ], # Scan Amount
	0x66 => [  22 => 's', 0x0400/100  ], # Pitch Mod Scan Freq.
	0x68 => [  23 => 's', 0x0400/100  ], # Vol Mod Scan Freq.
	0x6a => [  24 => 's', 0x0400/100  ], # Pitch Mod Scan Amount
	0x6c => [  25 => 's', 0x0400/100  ], # Vol Mod Scan Amount
	0x6e => [  30 => 's', 0x0400/100  ], # Pitch Mod Resonance
	0x70 => [  26 => 'S', 0x0400/100  ], # Vol Mod Volume
	0x72 => [ 103 => 'S', 0x4000/100  ], # Preset Volume
#	0x74 .. <EOF> => ?
);

sub import_data
{
	my (undef,$nr,$data) = @_;
	my %import;
	while ( my($offset,$rules) = each %IMPORT )
	{
		my ($CC,$pack,$calc) = @$rules;
		if ( $pack =~ /Z/ )
		{
			$import{$CC} = [ unpack $pack, substr $data, $offset ];
		}
		else
		{
			my $value = unpack $pack, substr $data, $offset;
			$value /= $calc if $calc;
			push @{$import{$CC}}, $value;
		}
	}
	my %data;
	$data{$_->{CC}} = $_->value_import(@{$import{$_->{CC}}}) for values %CONTROLLER;
	$data{_nr} = $nr if defined $nr;
	\%data;
}


sub sysex
{
	my(undef,$s) = @_;
	my $h;
	if ( $s =~ /^\xf0.*\xf7$/s )
	{
		$h = substr $s, 1, 22;
		$s = substr $s, 23, -1;
	}
	else
	{
		$h = substr $s, 0, 22;
		$s = substr $s, 22;
	}
	my $l;
	if    ( $h =~ /^..\x01\x01/ )
	{
		$l = length($s) / 32;
	}
	elsif ( $h =~ /^..\x04\x01/ )
	{
		$l = length($s);
	}
	elsif ( $h =~ /^..\x05\x01/ )
	{
		$l = length($s);
	}
	else
	{
		die sprintf "Sysex header: %s", unpack 'H*', $h;
	}
	my @data;
	while ( length $s )
	{
		die "Sysex too short: %s", unpack 'H*', $s if $l > length $s;
		push @data, &import_data(undef,undef, join '', map {pack'v',&_sx_(hex unpack'H*',$_)} substr($s,0,$l) =~ /.../gs );
		$s = substr $s, $l;
	}
	die sprintf "Sysex too long: %s", unpack 'H*', $s if length $s;
	\@data;
}

sub _sx_
{
	my($v) = @_;
	my $r = $v & 0x3f;
	$v &= 0xfffc0;
	$v >>= 2;
	$r |= $v & 0x3fff;
	$v &= 0x3c000;
	$v >>= 2;
	$v | $r;
}


1;
