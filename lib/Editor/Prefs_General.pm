
package ThereMaxi::Editor::Prefs::General;

use strict;
use warnings;


sub new
{
	sub box
	{
		my $vbox = Gtk2::VBox->new(0);
		$vbox->add($_) for @_;
		$vbox;
	}
	my $grp = Gtk2::SizeGroup->new('both');
	my $vbox = Gtk2::VBox->new(0);
	$vbox->pack_start(Gtk2::Frame->new_with_object(&box(&_editor_($grp)),'Editor'),0,0,10);
	$vbox->pack_start(Gtk2::Frame->new_with_object(&box(&_device_($grp)),'Device'),0,0,10);
	($vbox,Gtk2::Label->new('General'));
}


sub _editor_
{
	my($grp) = @_;
	my @box;
	{
		push @box, my $hbox = Gtk2::HBox->new(0);
		$hbox->pack_start(my $lbl = Gtk2::Label->new('Library Directory'),0,0,10);
		$lbl->set_alignment(0,0.4);
		$grp->add_widget($lbl);
		$hbox->pack_end(my $name = Gtk2::Label->new($main::STATE{editor}->{library_path}),0,0,10);
		$hbox->pack_start(my $btn = Gtk2::FileChooserButton->new('Library Directory','select-folder'),0,0,0);
		$btn->set_show_hidden(0);
		$btn->set_create_folders(1);
		$btn->set_filename($main::STATE{editor}->{library_path});
		$btn->signal_connect(selection_changed=>sub{ $name->set_text($main::STATE{editor}->{library_path}=$btn->get_filename) });
	}
	@box;
}


sub _device_
{
	my($grp) = @_;
	my @box;
	{
		push @box, my $hbox = Gtk2::HBox->new(0);
		$hbox->pack_start(my $lbl = Gtk2::Label->new('Automatic Discovery'),0,0,10);
		$lbl->set_alignment(0,0.4);
		$grp->add_widget($lbl);
		$hbox->pack_start(my $btn = Gtk2::CheckButton->new,0,0,0);
		$btn->set_active($main::STATE{device}->{discovery_interval});
		$btn->signal_connect(toggled=>sub{ThereMaxi::Event->fire('prefs/discovery_interval', $main::STATE{device}->{discovery_interval} = $btn->get_active )});
	}
	{
		push @box, my $hbox = Gtk2::HBox->new(0);
		$hbox->pack_start(my $lbl = Gtk2::Label->new('MIDI Read Timeout'),0,0,10);
		$lbl->set_alignment(0,0.4);
		$grp->add_widget($lbl);
		$hbox->pack_start(my $btn = Gtk2::SpinButton->new_with_range(0,60,1),0,0,0);
		$hbox->pack_start(Gtk2::Label->new('Seconds'),0,0,0);
		$btn->set_value($main::STATE{device}->{midi_read_timeout});
		$btn->signal_connect(value_changed=>sub{ $main::STATE{device}->{midi_read_timeout} = $btn->get_value });
	}
	push @box, Gtk2::HSeparator->new;
	my $mi = sub
	{
		my($mi) = @_;
		push @box, my $hbox = Gtk2::HBox->new(0);
		$hbox->pack_start(my $lbl = Gtk2::Label->new("MIDI Input ".$mi),0,0,10);
		$lbl->set_alignment(0,0.4);
		$grp->add_widget($lbl);
		my @btn =
		(
			[map{sprintf 'Channel %d'   ,$_} 0..15],
			[map{sprintf 'Controller %d',$_} 0..127],
			[qw( 7bit 14bit )],
		);
		for my $nr ( 0 .. $#btn )
		{
			$hbox->pack_start(my $btn = Gtk2::ComboBox->new_text,0,0,0);
			$btn->append_text($_) for @{$btn[$nr]};
			$btn->set_active($main::STATE{device}->{midi_input}->{lc$mi}->[$nr]);
			$btn->signal_connect(changed=>sub{ ThereMaxi::Event->fire('prefs/midi_input',lc$mi,$nr, $main::STATE{device}->{midi_input}->{lc$mi}->[$nr] = $btn->get_active ) });
		}
#		$hbox->pack_end(Gtk2::Label->new_with_markup('<i>Changes require Restart</i>'),0,0,10);
	};
	&$mi('Volume');
	&$mi('Pitch');
	@box;
}


1;
