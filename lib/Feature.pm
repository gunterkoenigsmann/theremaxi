
package ThereMaxi::Feature;

use strict;
use warnings;

require "$main::LIB/Feature/MidiFeedbackLoop.pm";


sub list
{(
	[ThereMaxi::Feature::MidiFeedbackLoop->new,'MidiFeedbackLoop'],
)}


sub select_controller
{
	shift;
	my $cb = shift;
	my @C = grep {!$_->{FEATURE}} @_;
	return unless @C;
	my $menu = Gtk2::Menu->new;
	foreach my $C ( @C )
	{
		$menu->append(my $item = Gtk2::MenuItem->new_with_label(sprintf'%d : %s',$C->{CC},$C->{name}));
		$item->signal_connect(activate=>sub{$menu->destroy;&$cb($C)});
	}
	$menu->show_all;
	my $evt = Gtk2->get_current_event;
	$menu->popup(undef,undef,undef,undef,$evt->button,$evt->time);
}


1;
