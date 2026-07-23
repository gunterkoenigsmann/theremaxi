#!/usr/bin/perl -w

# Writes the protocol knowledge of the perl implementation to two JSON files:
#
#   protocol/tables.json  - what the Theremini's parameters are: CC numbers,
#                           names, ranges, units, enum labels, and the byte
#                           offsets a sysex preset dump is decoded from.
#   protocol/golden.json  - what the perl code *does* with them: decoded preset
#                           dumps and value-to-wire conversions, to test a
#                           reimplementation against.
#
# Both files are generated, committed, and checked for drift by t/check.sh, so
# a change to the tables in lib/ that is not regenerated fails the build. The
# perl code stays the authoritative source until something else can reproduce
# these files byte for byte.
#
# Usage: tools/dump-protocol.pl [output directory]

use strict;
use warnings;

use FindBin;
use JSON::PP;
use MIME::Base64;
use File::Temp qw( tempdir );

our $NAME = 'ThereMaxi';
our $CWD  = $FindBin::Bin.'/..';
our $LIB  = "$CWD/lib";
our %STATE =
(
	device => { discovery_interval=>0, midi_read_timeout=>10,
	            midi_input=>{ volume=>[1,2,0], pitch=>[1,20,0] } },
	editor => { library_path=>tempdir(CLEANUP=>1) },
);

my $out = shift || "$CWD/protocol";

require "$LIB/Storage.pm";
require "$LIB/Event.pm";
require "$LIB/Controller.pm";
require "$LIB/Preset.pm";
require "$LIB/Library.pm";
require "$LIB/Sysex.pm";

{
	no warnings 'once';
	*ThereMaxi::Device::init           = sub {};
	*ThereMaxi::Device::sync_on_change = sub { 0 };
	*ThereMaxi::Device::value_changed  = sub { 0 };
}
ThereMaxi::Preset->init;

# list_tunables only reports controllers that have been instantiated, because
# the "tunable" flag is set in Controller::new. Preset->init creates the ones
# belonging to a preset; without this the global parameters - master volume and
# the note range - would be missing from the generated table entirely.
{
	no warnings 'once';
	ThereMaxi::Controller->new($_) for keys %ThereMaxi::Controller::CONTROLLER;
}


# ---------------------------------------------------------------- tables ---

my %controller;
for my $c ( ThereMaxi::Controller->list_tunables, ThereMaxi::Controller->list_in_preset )
{
	my $CC = $c->{CC};
	next if $controller{$CC};

	my $class = ref $c;
	$class =~ s/^ThereMaxi::Controller:://;

	my %e =
	(
		cc         => $CC,
		name       => $c->{name},
		class      => $class,
		in_preset  => $c->{preset} ? JSON::PP::true : JSON::PP::false,
	);

	if ( $CC =~ /^\d+$/ )
	{
		# CC numbers below 32 whose range does not fit in 7 bits are sent as a
		# pair: the CC itself carries the high bits, CC+32 the low ones.
		my $wide = $CC < 32 && defined($c->{min}) && defined($c->{max})
		           && abs($c->{min})+abs($c->{max}) > 0x7f;
		$e{bits} = $wide ? 14 : 7;
		$e{lsb_cc} = $CC + 32 if $wide;
	}

	$e{$_} = $c->{$_} + 0 for grep { defined $c->{$_} } qw( min max dig );
	$e{format} = $c->{fmt} if defined $c->{fmt};
	$e{step}   = $c->{step} + 0 if defined $c->{step};

	if ( $c->{show} )
	{
		$e{ui} =
		{
			order => $c->{show}->[0] + 0,
			tab   => $c->{show}->[1],
			group => $c->{show}->[2],
			label => $c->{show}->[3],
		};
	}

	# Which value_import is actually in effect, resolved through the class
	# hierarchy rather than guessed from the class name. An unknown one stops
	# the generator instead of being silently ignored.
	my %IMPORT_KIND =
	(
		\&ThereMaxi::Controller::value_import          => 'identity',
		\&ThereMaxi::Controller::numeric::value_import => 'numeric',
		\&ThereMaxi::Controller::name::value_import    => 'text',
		\&ThereMaxi::Controller::_074::value_import    => 'cutoff',
		\&ThereMaxi::Controller::_090::value_import    => 'sum',
	);
	my $import = $IMPORT_KIND{ $c->can('value_import') };
	die "$CC: unknown value_import\n" unless $import;
	$e{import} = $import;

	$e{properties} = [ sort grep { $c->{props}->{$_} } keys %{$c->{props}||{}} ];
	$e{values}     = [ $c->range ] if $c->can('range') && $class !~ /^(numeric|prozent|_074|_090dep)$/;

	$controller{$CC} = \%e;
}

my %offset;
no warnings 'once';
while ( my($off,$rule) = each %ThereMaxi::Preset::IMPORT )
{
	my($CC,$pack,$divisor) = @$rule;
	$offset{ sprintf '0x%02x', $off } =
	{
		cc      => "$CC",
		pack    => $pack,
		divisor => defined($divisor) ? $divisor + 0 : JSON::PP::null,
	};
}

my %message = %ThereMaxi::Sysex::MESSAGE;
my %name_message;
while ( my($k,$v) = each %ThereMaxi::Sysex::NAME_MESSAGE )
{
	$name_message{$k} = { prefix => $v->{prefix}, suffix => $v->{suffix} };
}

my $tables =
{
	generated_by => 'tools/dump-protocol.pl',
	note         => 'Generated from lib/Controller.pm, lib/Preset.pm and lib/Sysex.pm. Do not edit by hand.',
	controllers  => \%controller,
	sysex        => {
		preset_length => 0x74,
		offsets       => \%offset,
	},
	messages      => \%message,
	name_messages => \%name_message,
	name_length   => 13,
};


# ---------------------------------------------------------------- golden ---

# A reproducible pseudo random generator, so the vectors do not depend on the
# perl version's rand().
my $seed = 20260723;
sub rnd { $seed = ( $seed * 1103515245 + 12345 ) & 0x7fffffff; $seed >> 7 }

my @blobs;
for my $case ( 0 .. 7 )
{
	my $blob = $case == 0 ? pack('C*', (0x00) x 0x76)
	         : $case == 1 ? pack('C*', (0xff) x 0x76)
	         :              pack('v*', map { &rnd % 0x4000 } 0 .. 0x74/2);
	# the preset name is stored padded - one byte per character - the effect
	# name is not. See the pack templates in %ThereMaxi::Preset::IMPORT.
	substr($blob,0x02,26) = pack '(a1x1)13', split //, sprintf '%-13s', "CASE $case";
	substr($blob,0x36,13) = pack 'Z13', 'Med Delay';

	my $data = ThereMaxi::Preset->import_data($case,$blob);
	push @blobs,
	{
		input   => encode_base64($blob,''),
		decoded => { map { $_ => &_scalar_($data->{$_}) } keys %$data },
	};
}

# Every controller's display value swept across its range, with the bytes that
# go on the wire for it. This is where a reimplementation's scaling is checked.
my %export;
for my $c ( ThereMaxi::Controller->list_tunables )
{
	my @sweep;
	if ( defined $c->{min} && defined $c->{max} )
	{
		for my $step ( 0 .. 32 )
		{
			my $value = $c->{min} + ( $c->{max} - $c->{min} ) * $step / 32;
			$value = sprintf '%.'.($c->{dig}||0).'f', $value;
			push @sweep, { value => $value + 0, wire => &_wire_($c,$value) };
		}
	}
	elsif ( $c->can('range') )
	{
		# enums: every selectable index
		my @range = $c->range;
		push @sweep, { value => $_ + 0, wire => &_wire_($c,$_) } for 0 .. $#range;
	}
	else
	{
		next;
	}
	$export{$c->{CC}} = \@sweep;
}

sub _wire_
{
	my($c,$value) = @_;
	my $wire = $c->value_export($value);
	ref($wire) ? [ map {$_+0} @$wire ] : [ $wire + 0 ];
}

# The 7-bit unpacking, on its own. Every input is three MIDI data bytes, so each
# is 0..0x7f - which is what the device actually sends and where the perl's bit
# arithmetic is defined.
my @sx;
for ( 1 .. 96 )
{
	my @triple = map { &rnd % 0x80 } 1 .. 3;
	push @sx,
	{
		in  => [ @triple ],
		out => ThereMaxi::Preset::_sx_( hex unpack 'H*', pack 'C3', @triple ),
	};
}

# Whole sysex dumps, framed the way the device sends them: an F0, a 22 byte
# header whose third and fourth bytes select the layout, the 7-bit-packed body,
# an F7. A preset is 174 packed bytes, which unpack to the 0x74 the decoder
# reads. Built here by running random bodies through the real Preset->sysex, so
# there is no need to invert the packing to make test data.
my $preset_packed = 174;
my @messages;
for my $spec ( [ '01', 32 ], [ '05', 1 ], [ '04', 1 ] )
{
	my($type,$count) = @$spec;
	my $header = pack('C*', 0x00, 0x00, hex $type, 0x01) . ("\x00" x 18);
	my $body   = pack 'C*', map { &rnd % 0x80 } 1 .. $preset_packed * $count;
	my $msg    = "\xf0" . $header . $body . "\xf7";

	my $decoded = ThereMaxi::Preset->sysex($msg);
	push @messages,
	{
		input   => encode_base64($msg,''),
		header  => $type,
		# Only the numeric values: the bodies are random, so the name fields
		# would be raw high bytes that add nothing here and make the JSON
		# fragile. Text decoding is covered by the preset vectors above, whose
		# names are controlled.
		presets => [ map { my $d = $_; +{ map { $_ => &_scalar_($d->{$_}) } grep { /^\d+$/ || $_ eq '_nr' } keys %$d } } @$decoded ],
	};
}

# The device control messages the app sends, as full on-the-wire bytes (F0 and
# F7 included, the way the ALSA sysex() call frames them). The name carrying
# ones are recorded for a spread of names that exercise trimming, truncation to
# 13 and padding.
my @control;
push @control,
{
	name  => 'identity_request',
	bytes => encode_base64("\xf0".ThereMaxi::Sysex->payload('identity_request')."\xf7",''),
};
push @control,
{
	name  => 'request_all_presets',
	bytes => encode_base64("\xf0".ThereMaxi::Sysex->payload('request_all_presets')."\xf7",''),
};
for my $n ( '', 'TEST', 'THIRTEEN CHAR', 'WAY TOO LONG A NAME', '  trimmed  ' )
{
	my $bytes = pack 'H*', join '', @{ ThereMaxi::Controller->get('_ps')->value_export($n) };
	push @control,
	{
		name    => 'write_preset_name',
		arg     => $n,
		bytes   => encode_base64("\xf0".ThereMaxi::Sysex->name_payload('write_preset_name',$bytes)."\xf7",''),
	};
}

my $golden =
{
	generated_by => 'tools/dump-protocol.pl',
	note         => 'Generated. Regenerate with tools/dump-protocol.pl after changing lib/.',
	presets      => \@blobs,
	export       => \%export,
	sx           => \@sx,
	messages     => \@messages,
	control      => \@control,
};

sub _scalar_
{
	my($v) = @_;
	return JSON::PP::null unless defined $v;
	return $v if $v =~ /[^0-9.eE+-]/;      # names and other strings
	# NaN and Inf are not JSON, and a value that reaches this point is one the
	# program would also have written into a library file. Fail loudly instead.
	die "not a finite number: $v\n" unless $v == $v && abs($v) != 9**9**9;
	$v + 0;
}


# ----------------------------------------------------------------- write ---

mkdir $out unless -d $out;
my $json = JSON::PP->new->utf8->pretty->canonical;
for my $f ( [ "$out/tables.json", $tables ], [ "$out/golden.json", $golden ] )
{
	open my $F, '>', $f->[0] or die "$f->[0]: $!";
	print $F $json->encode($f->[1]);
	close $F;
	printf "%s (%d bytes)\n", $f->[0], -s $f->[0];
}
