#!/usr/bin/perl -w

use strict;
use warnings;

our $NAME = 'ThereMaxi';

BEGIN
{
	my @need = grep {!eval"require $_"} qw( File::Pid FindBin Getopt::Long Gtk2 JSON::PP MIDI::ALSA MIME::Base64 sigtrap );
	die "Modules not found:\n\t",join("\n\t",@need),"\n\n" if @need;
}

use FindBin;
our $CWD   = $FindBin::Bin;
our $LIB   = "$CWD/lib";
my  $pid   = ( -w $ENV{XDG_RUNTIME_DIR} ? $ENV{XDG_RUNTIME_DIR} : $CWD )."/$NAME.pid";
my  $state = "$CWD/$NAME.state";
my  $clean = 0;

use Getopt::Long;
GetOptions
(
	'pidfile=s' => \$pid,
	'statefile=s' => \$state,
	'cleanstate' => \$clean,
)
or die "Usage: $0 [--pidfile=<filename>] [--statefile=<filename>] [--cleanstate]\n";

$pid = File::Pid->new({file=>$pid});
die "$NAME is running\n" if $pid->running;
$pid->write;

$SIG{__DIE__} = sub
{
	$pid->remove;
	print STDERR "$NAME died\n",@_;
	exit 1;
};
use sigtrap qw( die untrapped normal-signals error-signals old-interface-signals );

require "$LIB/Storage.pm";
our %STATE = $clean ? () : ThereMaxi::Storage->load($state);
{
	$STATE{device}->{discovery_interval} = 5 unless defined
	$STATE{device}->{discovery_interval};

	$STATE{device}->{midi_read_timeout} = 10 unless defined
	$STATE{device}->{midi_read_timeout};

	$STATE{device}->{midi_input}->{volume} = [1,2,0] unless defined
	$STATE{device}->{midi_input}->{volume};

	$STATE{device}->{midi_input}->{pitch} = [1,20,0] unless defined
	$STATE{device}->{midi_input}->{pitch};

	$STATE{editor}->{library_path} = "$CWD/data" unless defined
	$STATE{editor}->{library_path};

	mkdir $STATE{editor}->{library_path} unless
	   -d $STATE{editor}->{library_path};

	delete $STATE{online};
}

use Gtk2 q(-init);
require "$LIB/Editor.pm";
ThereMaxi::Editor->init;
Gtk2->main;

ThereMaxi::Storage->save($state,\%STATE);

$pid->remove;
exit 0;
