
package ThereMaxi::Editor::Prefs::Controller;

use strict;
use warnings;


sub new
{
	my @C = ThereMaxi::Controller->list_tunables;
	my @t = 
	(
		{ title=>'CC',           align=>1,   },
		{ title=>'Name',         align=>0,   },
		{ title=>'in Editor',    align=>0,   },
		{ title=>'Min Value',    align=>1,   },
		{ title=>'Max Value',    align=>1,   },
		{ title=>'SyncOnChange', align=>0.5, },
		{ title=>'MidiFeedback', align=>0.5, },
	);
	$_->{sizeGroup} = Gtk2::SizeGroup->new('horizontal') for @t;
	my $vbox = Gtk2::VBox->new(0);
	my $row = 0;
	my $attach = sub
	{
		my $hbox = Gtk2::HBox->new(0);
		for my $col ( 0 .. $#t )
		{
			next unless defined $_[$col];
			$_[$col] = Gtk2::Alignment->new_with_object( ref($_[$col]) ? $_[$col] : Gtk2::Label->new($_[$col]) ,$t[$col]->{align},0.5,0,0);
			$t[$col]->{sizeGroup}->add_widget($_[$col]);
			$hbox->pack_start($_[$col],0,0,5);
		}
		$hbox = Gtk2::EventBox->new_with_object($hbox);
		$hbox->modify_bg('normal',Gtk2::Gdk::Color->parse('grey90')) if $row % 2;
		$vbox->pack_start($hbox,0,0,0);
		$row++;
	};
	&$attach(map{Gtk2::Label->new_with_markup('<b>'.$_->{title}.'</b>')}@t);
	$vbox->pack_start(Gtk2::HSeparator->new,0,0,1);
	&$attach(&_makerow_($C[$_])) for 0 .. $#C;
	($vbox,Gtk2::Label->new('Controller'));
}


sub _makerow_
{
	my($self) = @_;
	my $B = sub
	{
		my($p) = @_;
		my $btn = Gtk2::CheckButton->new;
		$btn->set_sensitive( defined($self->{props}->{$p}) && !$self->{BLOCKED} );
		$btn->set_active( $main::STATE{controller}->{$p}->{$self->{CC}} || $self->{props}->{$p} );
		$btn->signal_connect(clicked=>sub{ $self->prefs_changed($p,$btn->get_active) });
		$btn;
	};
	(
		$self->{CC},
		$self->{name},
		$self->show_path,
		$self->range_lower,
		$self->range_upper,
		&$B('sync_on_change'),
		&$B('MidiFeedbackLoop'),
	);
}


1;
