
package ThereMaxi::Editor;

use strict;
use warnings;

require "$main::LIB/Event.pm";
require "$main::LIB/Controller.pm";
require "$main::LIB/Preset.pm";
require "$main::LIB/Library.pm";
require "$main::LIB/Device.pm";
require "$main::LIB/Editor/gtk.pm";
require "$main::LIB/Editor/Layout.pm";


sub init
{
	my $window = Gtk2::Window->new('toplevel');
	$window->set_title($main::NAME);
	$window->set_icon_from_file("$main::CWD/$main::NAME.icon") if -r "$main::CWD/$main::NAME.icon";
	$window->signal_connect(delete_event=>sub
	{
		$main::STATE{main}->{window}->{position} = [$window->get_position];
		$main::STATE{main}->{window}->{size} = [$window->get_size];
		Gtk2->main_quit;
	});
	$window->move(@{$main::STATE{main}->{window}->{position}}) if $main::STATE{main}->{window}->{position};
	$window->resize(@{$main::STATE{main}->{window}->{size}}) if $main::STATE{main}->{window}->{size};
	{
		my $layout = Gtk2::Alignment->new_with_object(ThereMaxi::Editor::Layout->new,0.5,0.5,1,1);
		$layout->set_padding(5,0,5,5);
		$window->add($layout);
	}
	$window->show_all;
	$window->set_focus;

	ThereMaxi::Event->connect('ERROR'=>sub
	{
		print STDERR join("\n",@_);shift;
		my $dlg = Gtk2::MessageDialog->new($window,'modal','error','close','ERROR');
		$dlg->format_secondary_text(join '', @_);
		$dlg->run;
		$dlg->destroy;
	});
	ThereMaxi::Preset->init;
}


my %STATUS;

sub statusbar
{
	$STATUS{bar} = Gtk2::Statusbar->new;
	$STATUS{ctx} = $STATUS{bar}->get_context_id('default');
	ThereMaxi::Event->connect('prefs/discovery_interval',sub{ &status( pop ? '' : 'Discovery disabled' ) });
	ThereMaxi::Event->connect('device/discover'=>sub{ &status(ThereMaxi::Device->info.' - '.( pop ? 'online' : 'offline' )) });
	$STATUS{bar};
}

sub status
{
	$STATUS{bar}->remove_all($STATUS{ctx});
	$STATUS{bar}->push($STATUS{ctx},pop);
}


1;
