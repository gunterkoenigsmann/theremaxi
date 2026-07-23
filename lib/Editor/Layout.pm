
package ThereMaxi::Editor::Layout;

use strict;
use warnings;

require "$main::LIB/Editor/Toolbar.pm";
require "$main::LIB/Editor/Presets.pm";
require "$main::LIB/Editor/Library.pm";
require "$main::LIB/Feature.pm";


sub new
{
	my $tabs = Gtk2::Notebook->new;

	$tabs->append_page(&_Basic);
	$tabs->append_page(&_Advanced);
	$tabs->append_page(&_Global);
	$tabs->append_page(@$_) for ThereMaxi::Feature->list;
	$tabs->show_all; # Die nächste Zeile braucht das !
	$tabs->set_current_page($main::STATE{main}->{layout}->{tabs}||0);
	$tabs->signal_connect(switch_page=>sub{ $main::STATE{main}->{layout}->{tabs} = $_[2] });

	my $pane = Gtk2::HPaned->new;
	$pane->add1(&_Main);
	$pane->add2($tabs);
	$pane->set_position($main::STATE{main}->{layout}->{pane}||-1);
	$pane->get_child1->signal_connect(size_allocate=>sub{ $main::STATE{main}->{layout}->{pane} = $pane->get_position });

	my $vbox = Gtk2::VBox->new(0);
	$vbox->pack_start(ThereMaxi::Editor::Toolbar->new,0,0,0);
	$vbox->pack_start($pane,1,1,0);
	$vbox->pack_end(ThereMaxi::Editor->statusbar,0,0,0);
	$vbox;
}


sub _Main
{
	my $group = { LabelSizeGroup => Gtk2::SizeGroup->new('both') };

	my $fbox = Gtk2::VBox->new(0);
	$fbox->pack_start(ThereMaxi::Controller->new('_ps',LAYOUT=>$group)->widget,0,0,0);
	$fbox->pack_start(ThereMaxi::Controller->new(103,LAYOUT=>$group)->widget,0,0,0);

	my $pane = Gtk2::HPaned->new;
	$pane->add(ThereMaxi::Editor::Library->selection);
	$pane->add(ThereMaxi::Editor::Presets->selection);
	$pane->set_position($main::STATE{main}->{selection}->{pane}||-1);
	$pane->get_child1->signal_connect(size_allocate=>sub{ $main::STATE{main}->{selection}->{pane} = $pane->get_position });

	my $vbox = Gtk2::VBox->new(0);
	$vbox->pack_start(Gtk2::Frame->new_with_object($fbox),0,0,0);
	$vbox->pack_end( $pane ,1,1,0);

	$vbox;
}


sub _Basic
{(
	&_odd_
	(
		[ undef, [85,86],[84]       ],
		[ undef, [90]   ,[9,20,21]  ],
		[ undef, [80]   ,[74,71,29] ],
		[ undef, ['_fx'],[12,14,91] ],
	),
	Gtk2::Label->new('Basic')
)}


sub _Advanced
{(
	&_odd_
	(
		[ 'Volume Antenna', [],[26,25,23,27,28] ],
		[ 'Pitch Antenna',  [],[24,22,30]       ],
		[ undef,            [],[102]            ],
	),
	Gtk2::Label->new('Advanced')
)}


sub _Global
{(
	&_even_
	(
		[ undef, [87,88],[]  ],
		[ undef, []     ,[7] ],
	),
	Gtk2::Label->new('Global')
)}


sub _odd_
{
	my $gLeft =
	{
		LabelSizeGroup => Gtk2::SizeGroup->new('both'),
		ComboSizeGroup => Gtk2::SizeGroup->new('both'),
	};
	my $gRight =
	{
		LabelSizeGroup => Gtk2::SizeGroup->new('both'),
	};
	&_box_($gLeft,$gRight,@_);
}


sub _even_
{
	my $gBoth =
	{
		LabelSizeGroup => Gtk2::SizeGroup->new('both'),
		ComboSizeGroup => Gtk2::SizeGroup->new('both'),
	};
	&_box_($gBoth,$gBoth,@_);
}


sub _box_
{
	my($gLeft,$gRight,@box) = @_;
	my $vbox = Gtk2::VBox->new(0);
	foreach my $box ( @box )
	{
		my($label,$left,$right) = @$box;

		my $lbox = Gtk2::VBox->new(0);
		$lbox->pack_start(ThereMaxi::Controller->new($_,LAYOUT=>$gLeft)->widget,0,0,0) for @$left;

		my $rbox = Gtk2::VBox->new(0);
		$rbox->pack_start(ThereMaxi::Controller->new($_,LAYOUT=>$gRight)->widget,1,1,0) for @$right;

		my $hbox = Gtk2::HBox->new(0);
		$hbox->pack_start($lbox,0,0,0);
		$hbox->pack_end($rbox,1,1,0);

		$vbox->pack_start(Gtk2::Frame->new_with_object($hbox,$label),0,0,0);
	}
	$vbox;
}


1;
