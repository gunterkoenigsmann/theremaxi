#!/usr/bin/perl -w

# Loads the whole non-GUI part of ThereMaxi and exercises it: every controller
# class is instantiated, a preset is decoded through the sysex offset table,
# every value is exported to MIDI wire format and a library is written to disk
# and read back.
#
# Needs nothing but core perl - no Gtk2, no MIDI::ALSA, no hardware - because
# the controller classes touch Gtk2 only inside widget() and the device is
# stubbed out below. Per-file "perl -c" cannot replace this: the Controller
# subclasses use base on packages that live in files @INC knows nothing about,
# so they only compile when loaded in order.

use strict;
use warnings;

use FindBin;
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

require "$LIB/Storage.pm";
require "$LIB/Event.pm";
require "$LIB/Controller.pm";
require "$LIB/Preset.pm";
require "$LIB/Library.pm";
require "$LIB/Sysex.pm";

# The device needs ALSA and hardware, so replace it. Preset->init calls init,
# and saving a preset goes through save_preset/sync_on_change.
{
	no warnings 'once';
	*ThereMaxi::Device::init           = sub {};
	*ThereMaxi::Device::sync_on_change = sub { 0 };
	*ThereMaxi::Device::value_changed  = sub { 0 };
	*ThereMaxi::Device::save_preset    = sub { 0 };
	*ThereMaxi::Device::send_preset    = sub { 1 };
}

my $fail = 0;
sub ok
{
	my($cond,$what) = @_;
	print $cond ? "ok   - $what\n" : "FAIL - $what\n";
	$fail++ unless $cond;
}

ThereMaxi::Preset->init;

# 1. every controller class loads and instantiates
my @C = ThereMaxi::Controller->list_in_preset;
ok( @C == 27, sprintf 'controllers in preset: %d', scalar @C );
ok( ThereMaxi::Controller->list_tunables > 0, 'tunable controllers listed' );

# 2. decode a preset through the sysex offset table. The Theremini stores names
#    with a padding byte after each character, hence the (Zx)13 unpacking.
my $blob = pack 'v*', map { $_ * 137 % 0x4000 } 0 .. 0x74/2;
substr($blob,0x02,26) = pack '(a1x1)13', split //, 'TESTPRESET   ';
my $data = ThereMaxi::Preset->import_data(3,$blob);
ok( $data->{_nr} == 3,             'preset number taken from the caller' );
ok( $data->{_ps} eq 'TESTPRESET',  'preset name decoded: '.$data->{_ps} );
ok( defined $data->{12},           'delay time decoded: '.($data->{12}//'undef') );
ok( defined $data->{74},           'filter cutoff decoded: '.($data->{74}//'undef') );

# 3. a filter cutoff word below 15 used to decode to NaN, which slipped through
#    the range check, was written to the library file as invalid JSON, and made
#    that library unloadable for good.
{
	my $low = $blob;
	substr($low,0x56,2) = pack 'v', 3;
	my $cutoff = ThereMaxi::Preset->import_data(0,$low)->{74};
	ok( defined $cutoff && $cutoff == $cutoff, "cutoff word 3 decodes to a number: ".($cutoff//'undef') );

	my $json = eval { ThereMaxi::Storage->save("$STATE{editor}->{library_path}/nan.json",{74=>$cutoff}); 1 };
	ok( $json, 'and survives a trip through JSON' );
	ok( eval { ThereMaxi::Storage->load("$STATE{editor}->{library_path}/nan.json"); 1 }, 'and can be read back' );
}

# 4. export every value back to MIDI wire format
my $exported = 0;
for my $c ( @C )
{
	$c->set_value($data->{$c->{CC}});
	my $e = $c->value_export;
	$exported++ if defined $e;
}
ok( $exported == @C, "exported $exported of ".@C.' values' );

# 5. library round-trip through JSON storage
ThereMaxi::Library->init;
ThereMaxi::Library->new(1,'smoketest');
ThereMaxi::Library->new_preset('smoketest','TESTPRESET');
ThereMaxi::Library->save_preset('smoketest',0,ThereMaxi::Preset->get_values);
my %back = ThereMaxi::Library->load_preset('smoketest',0);
ok( keys(%back) == @C,          'round-trip keeps all '.@C.' values' );
ok( $back{_ps} eq 'TESTPRESET', 'round-trip keeps the preset name' );

# 6. the device control messages, pinned to the exact bytes Device.pm's old
#    literals produced, so lib/Sysex.pm can be trusted without a Theremini.
{
	my $hex = sub { unpack 'H*', $_[0] };

	ok( $hex->(ThereMaxi::Sysex->payload('identity_request')) eq '7e7f0601',
		'identity request payload' );
	ok( $hex->(ThereMaxi::Sysex->payload('request_all_presets')) eq '040b06030000000000000000000000',
		'request-all-presets payload' );

	# the name field is the 13 raw bytes value_export produces
	my $name_bytes = sub { pack 'H*', join '', @{ ThereMaxi::Controller->get('_ps')->value_export($_[0]) } };

	ok( $hex->(ThereMaxi::Sysex->name_payload('write_preset_name',$name_bytes->('TEST'))) eq
		'040b0607000000000000000000000001'.'54455354202020202020202020'.'2000',
		'write-preset-name payload for TEST' );

	my $fx_bytes = sub { pack 'H*', join '', @{ ThereMaxi::Controller->get('_fx')->value_export($_[0]) } };
	ok( $hex->(ThereMaxi::Sysex->name_payload('write_effect_name',$fx_bytes->('Off'))) =~ /^040b0608/,
		'write-effect-name payload framing' );
}

print $fail ? "\n$fail check(s) failed\n" : "\nall checks passed\n";
exit( $fail ? 1 : 0 );
