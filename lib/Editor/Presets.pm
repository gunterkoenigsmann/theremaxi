
package ThereMaxi::Editor::Presets;

use strict;
use warnings;


sub selection
{
	my %cols=
	(
		path => 0,
		name => 1,
	);
	my $list = Gtk2::ListStore->new(qw( Glib::String Glib::String ));
	my $view = Gtk2::TreeView->new($list);
	{
		my $column = Gtk2::TreeViewColumn->new;
		my $renderer = Gtk2::CellRendererText->new;
		$column->set_title('Theremini Presets');
		$column->pack_start($renderer,1);
		$column->add_attribute($renderer,text=>$cols{name});
		$view->append_column($column);
	}
	ThereMaxi::Event->connect('sync/presets'=>sub
	{
		$list->clear;
		my @lib = ThereMaxi::Library->list_presets('.');
		for ( my$nr=0; $nr<=$#lib; $nr++ )
		{
			my $iter = $list->append;
			$list->set($iter,$cols{path}=>"./$nr");
			$list->set($iter,$cols{name}=>sprintf '%02d %s',$nr+1,$lib[$nr]);
			if ( ($main::STATE{preset}||'') eq "./$nr" )
			{
				$view->get_selection->select_iter($iter);
			}
		}
	});
	$view->get_selection->set_mode('browse');
	$view->get_selection->signal_connect(changed=>sub
	{
		return unless my $iter = $_[0]->get_selected;
		return unless my $path = $list->get_value($iter,$cols{path});
		ThereMaxi::Preset->load($main::STATE{online}=$path) if
		ThereMaxi::Device->select_preset(substr $path, 2) >= 0;
		ThereMaxi::Event->fire('switch/presets');
	});

	Gtk2::ScrolledWindow->new_with_object($view,qw( never automatic ));
}


1;
