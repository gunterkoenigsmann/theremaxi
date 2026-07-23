
package ThereMaxi::Editor::Library;

use strict;
use warnings;


sub selection
{
	my %cols=
	(
		path => 0,
		name => 1,
	);
	my $tree = Gtk2::TreeStore->new(qw( Glib::String Glib::String ));
	my $view = Gtk2::TreeView->new($tree);
	{
		my $column = Gtk2::TreeViewColumn->new;
		my $renderer = Gtk2::CellRendererText->new;
		$column->set_title ('Libraries/Presets');
		$column->pack_start($renderer,1);
		$column->add_attribute($renderer,text=>$cols{name});
		$view->append_column($column);
	}
	ThereMaxi::Event->connect('sync/library'=>sub
	{
		$tree->clear;
		foreach my $lib ( ThereMaxi::Library->list_library )
		{
			my $iterL = $tree->append(undef);
			$tree->set($iterL,$cols{name}=>$lib);
			if ( ($main::STATE{preset}||'') eq $lib )
			{
				$view->expand_to_path($tree->get_path($iterL));
				$view->get_selection->select_iter($iterL);
			}
			my @lib = ThereMaxi::Library->list_presets($lib);
			for ( my$nr=0; $nr<=$#lib; $nr++ )
			{
				my $iterP = $tree->append($iterL);
				$tree->set($iterP,$cols{path}=>"$lib/$nr");
				$tree->set($iterP,$cols{name}=>sprintf '%02d %s',$nr+1,$lib[$nr]);
				if ( ($main::STATE{preset}||'') eq "$lib/$nr" )
				{
					$view->expand_to_path($tree->get_path($iterL));
					$view->get_selection->select_iter($iterP);
				}
			}
		}
	});
	$view->get_selection->set_mode('browse');
	$view->get_selection->signal_connect(changed=>sub
	{
		return unless my $iter = $_[0]->get_selected;
		my $path;
		if ( $path = $tree->get_value($iter,$cols{path}) )
		{
			ThereMaxi::Preset->load($path);
		}
		else
		{
			$path = $tree->get_value($iter,$cols{name});
		}
		ThereMaxi::Event->fire('switch/library',split '/', $path);
	});

	Gtk2::ScrolledWindow->new_with_object($view,qw( automatic automatic ));
}


1;
