
package ThereMaxi::Editor::Prefs;

use strict;
use warnings;

require "$main::LIB/Editor/Prefs_General.pm";
require "$main::LIB/Editor/Prefs_Controller.pm";


sub window
{
	my($caller) = @_;
	my $window = Gtk2::Window->new('toplevel');
	$window->set_title($main::NAME.'/Preferences');
	$window->set_transient_for($caller->get_toplevel);
	$window->set_modal(1);
	$window->signal_connect(delete_event=>sub
	{
		$main::STATE{prefs}->{window}->{position} = [$window->get_position];
		$main::STATE{prefs}->{window}->{size} = [$window->get_size];
		$window->destroy;
	});
	$window->move(@{$main::STATE{prefs}->{window}->{position}}) if $main::STATE{prefs}->{window}->{position};
	$window->resize(@{$main::STATE{prefs}->{window}->{size}}) if $main::STATE{prefs}->{window}->{size};
	{
		sub align
		{
			$_[0] = Gtk2::Alignment->new_with_object($_[0],0.5,0.5,1,1);
			$_[0]->set_padding(5,5,5,5);
			@_;
		}
		my $tabs = Gtk2::Notebook->new;
		$tabs->append_page(align(ThereMaxi::Editor::Prefs::Controller->new));
		$tabs->append_page(align(ThereMaxi::Editor::Prefs::General->new));
		$tabs->show_all; # Die nächste Zeile braucht das !
		$tabs->set_current_page($main::STATE{prefs}->{layout}->{tabs}||0);
		$tabs->signal_connect(switch_page=>sub{ $main::STATE{prefs}->{layout}->{tabs} = $_[2] });
		$window->add(align($tabs));
	}
	$window->show_all;
	$window->set_focus;
}


1;
