
package ThereMaxi::Feature::MidiFeedbackLoop;

use strict;
use warnings;

use base 'ThereMaxi::Feature';

my %CONTROLLER;
my %RUN = (volume=>0,pitch=>0);
my %max =
(
	volume => $main::STATE{device}->{midi_input}->{volume}->[2] ? 0x3fff : 0x7f,
	pitch => $main::STATE{device}->{midi_input}->{pitch}->[2] ? 0x3fff : 0x7f
);
my %output;
my %lastval;


sub new
{
	my @CONTROLLER = ThereMaxi::Controller->list_with_prop('MidiFeedbackLoop');
	return Gtk2::Label->new('No Controllers available') unless @CONTROLLER;
	%CONTROLLER = map {$_->{CC}=>$_} @CONTROLLER;
	my @cols = ('Act.','Low','High','Sens.','Output','Rev.','Min','Max','');
	my $frame = sub
	{
		my($type,$text) = @_;
		my $table = Gtk2::Table->new(1,9,0);
		my $lbl = Gtk2::HBox->new;
		{
			$lbl->pack_start(my $btn = Gtk2::ToolButton->new_from_stock('gtk-add') ,0,0,0);
			$btn->signal_connect(clicked=>sub{ __PACKAGE__->select_controller(sub{ &_addrow_($table,$type,@_) },@CONTROLLER) });
		}
		$lbl->pack_start(Gtk2::Label->new($text),0,0,0);
		{
			$lbl->pack_start(my $btn = Gtk2::ToggleToolButton->new_from_stock('gtk-media-play') ,0,0,0);
			$btn->set_active(0);
			$btn->set_sensitive(0);
			$btn->signal_connect(toggled=>sub
			{
				$btn->set_stock_id( $btn->get_active ? 'gtk-media-stop' : 'gtk-media-play' );
				&_run_($type,$btn->get_active);
			});
			$btn->signal_connect_after(state_changed=>sub
			{
				$btn->set_stock_id('gtk-media-play') unless $btn->get_sensitive;
				&_run_($type,0) unless $btn->get_sensitive;
			});
			my $disc = 0;
			ThereMaxi::Event->connect
			(
				'device/discover' => sub
				{
					$disc = pop;
					ThereMaxi::Event->fire('feature/MidiFeedbackLoop',run=>$type,$RUN{$type});
				},
				'feature/MidiFeedbackLoop' => sub
				{
					return unless $_[1] eq 'run' && $_[2] eq $type;
					my $sens = $disc && $_[3];
					$btn->set_sensitive($sens);
					$btn->set_active(0) unless $sens;
				}
			);
		}
		$main::STATE{MidiFeedbackLoop}->{controller}->{$type} ||= {};
		$table->attach(Gtk2::Label->new_with_markup("<b>$cols[$_]</b>"),$_,$_+1,0,1,'expand','shrink',0,0) for 0 .. $#cols;
		&_addrow_($table,$type,$CONTROLLER{$_}) for keys %{$main::STATE{MidiFeedbackLoop}->{controller}->{$type}};
		Gtk2::Frame->new_with_object(Gtk2::ScrolledWindow->new_with_viewport($table,qw( automatic automatic )),$lbl);
	};
	ThereMaxi::Event->connect('prefs/midi_input'=>sub{ $max{$_[1]} = $_[3] ? 0x3fff : 0x7f if $_[2] == 2 });
	my $vbox = Gtk2::VBox->new;
	$vbox->pack_start(my $pane = Gtk2::VPaned->new,1,1,0);
	$pane->add1(&$frame(volume=>'Volume Antenna'));
	$pane->add2(&$frame(pitch=>'Pitch Antenna'));
	$pane->set_position($main::STATE{MidiFeedbackLoop}->{pane}||-1);
	$pane->get_child1->signal_connect(size_allocate=>sub{ $main::STATE{MidiFeedbackLoop}->{pane} = $pane->get_position });
	$vbox;
}


sub _addrow_
{
	my($table,$type,$C) = @_;
	$C->{FEATURE} = 'MidiFeedbackLoop';
	my $max = $max{$type};
	$main::STATE{MidiFeedbackLoop}->{controller}->{$type}->{$C->{CC}} ||= {};
	my $cc = $main::STATE{MidiFeedbackLoop}->{controller}->{$type}->{$C->{CC}};
	my $range = sub
	{
		my($what,$value) = @_;
		if ( $what )
		{
			$cc->{$what} = sprintf '%.'.$C->{dig}.'f', $value;
			$cc->{$what} =~ tr/,/./;
		}
		$output{$C->{CC}} = [ map { $_ * ( $cc->{max} - $cc->{min} ) / $max + $cc->{min} } 0 .. $max ] if defined $cc->{max} && defined $cc->{min};
	};
	my($rows,$cols) = $table->get_size;
	$table->resize($rows+1,$cols);
	my $col = 0;
	my @attach;
	my $attach = sub{ $table->attach($_[0],$col,$col+1,$rows,$rows+1,'expand','shrink',0,0); $col++; push @attach,$_[0] };
	{
		&$attach(my $itm = Gtk2::CheckButton->new);
		$itm->set_active($cc->{enabled}||=0);
		$itm->signal_connect(toggled=>sub{ ThereMaxi::Event->fire('feature/MidiFeedbackLoop',run=>$type, $RUN{$type} += ( $cc->{enabled} = $itm->get_active ) ? 1 : -1 ) });
		ThereMaxi::Event->fire('feature/MidiFeedbackLoop',run=>$type,++$RUN{$type}) if $cc->{enabled};
	}{
		&$attach(my $itm = Gtk2::SpinButton->new_with_range(0,$max,1));
		$itm->set_value($cc->{low}||=0);
		$itm->signal_connect(value_changed=>sub{ $cc->{low} = $itm->get_value });
		$itm->set_wrap(1);
	}{
		&$attach(my $itm = Gtk2::SpinButton->new_with_range(0,$max,1));
		$itm->set_value($cc->{high}||=$max);
		$itm->signal_connect(value_changed=>sub{ $cc->{high} = $itm->get_value });
		$itm->set_wrap(1);
	}{
		&$attach(my $itm = Gtk2::SpinButton->new_with_range(0,$max,1));
		$itm->set_value($cc->{sens}||=0);
		$itm->signal_connect(value_changed=>sub{ $cc->{sens} = $itm->get_value });
		$itm->set_wrap(1);
	}{
		&$attach(my $itm = Gtk2::Label->new($C->{name}));
	}{
		&$attach(my $itm = Gtk2::CheckButton->new);
		$itm->set_active($cc->{revert}||=0);
		$itm->signal_connect(toggled=>sub{ $cc->{revert} = $itm->get_active })
	}{
		&$attach(my $itm = Gtk2::SpinButton->new_with_range($C->{min},$C->{max},$C->{step}));
		$itm->set_value($cc->{min}||=$C->{min});
		$itm->signal_connect(value_changed=>sub{ &$range(min=>$itm->get_value) });
		$itm->set_digits($C->{dig});
		$itm->set_wrap(1);
	}{
		&$attach(my $itm = Gtk2::SpinButton->new_with_range($C->{min},$C->{max},$C->{step}));
		$itm->set_value($cc->{max}||=$C->{max});
		$itm->signal_connect(value_changed=>sub{ &$range(max=>$itm->get_value) });
		$itm->set_digits($C->{dig});
		$itm->set_wrap(1);
	}{
		&$attach(my $btn = Gtk2::ToolButton->new_from_stock('gtk-remove'));
		$btn->signal_connect(clicked=>sub
		{
			$_->destroy for @attach;
			$table->resize($rows,$cols);
			$table->show_all;
			delete $C->{FEATURE};
			delete $main::STATE{MidiFeedbackLoop}->{controller}->{$type}->{$C->{CC}};
			ThereMaxi::Event->fire('feature/MidiFeedbackLoop',run=>$type,--$RUN{$type});
		});
	}
	&$range();
	$table->show_all;
}


sub _run_
{
	my($type,$run) = @_;
	$run = $RUN{$type} if $run;
	ThereMaxi::Device->CC('MidiFeedbackLoop',$type => $run ? \&_loop_ : undef );
}


sub _loop_
{
	my($type,$value) = @_;
	while ( my($cc,$fb) = each %{$main::STATE{MidiFeedbackLoop}->{controller}->{$type}} )
	{
		next unless $fb->{enabled};
		next if $value < $fb->{low};
		next if $value > $fb->{high};
		my $last = $lastval{$cc} || $value; $lastval{$cc} = $value;
		next if abs( $last - $value ) < $fb->{sens};
		$value = $max{$type} - $value if $fb->{revert};
		ThereMaxi::Device::midi_cc($cc,$CONTROLLER{$cc}->value_export($output{$cc}->[$value]));
	}
}


1;
