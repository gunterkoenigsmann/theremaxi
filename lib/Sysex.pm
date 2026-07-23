
package ThereMaxi::Sysex;

use strict;
use warnings;

# The device control messages, as payloads without the enclosing F0/F7 that the
# ALSA sysex() call adds. These used to be qw(...) literals scattered through
# Device.pm; they live here as data so tools/dump-protocol.pl can hand them to
# the C port instead of anyone retyping them. t/smoke.pl pins the assembled
# bytes, so this cannot drift from what the device actually expects.

our %MESSAGE =
(
	identity_request    => [ 0x7e, 0x7f, 0x06, 0x01 ],
	request_all_presets => [ 0x04, 0x0b, 0x06, 0x03, (0) x 11 ],
);

# Messages that carry a 13-byte name field between a fixed prefix and suffix.
our %NAME_MESSAGE =
(
	write_preset_name => { prefix => [ 0x04, 0x0b, 0x06, 0x07, (0) x 11, 0x01 ], suffix => [ 0x20, 0x00 ] },
	write_effect_name => { prefix => [ 0x04, 0x0b, 0x06, 0x08, (0) x 11, 0x01 ], suffix => [ 0x20, 0x00 ] },
);


sub payload
{
	my(undef,$name) = @_;
	die "no such message: $name" unless $MESSAGE{$name};
	pack 'C*', @{$MESSAGE{$name}};
}


# $bytes is the 13 raw name bytes, as produced by the _ps / _fx controller's
# value_export.
sub name_payload
{
	my(undef,$name,$bytes) = @_;
	my $m = $NAME_MESSAGE{$name} or die "no such message: $name";
	pack('C*',@{$m->{prefix}}) . $bytes . pack('C*',@{$m->{suffix}});
}


1;
