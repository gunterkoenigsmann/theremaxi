
package ThereMaxi::Editor::Toolbar;

use strict;
use warnings;

require "$main::LIB/Editor/Library_Manager.pm";
require "$main::LIB/Editor/Prefs.pm";


sub new
{
	my $hbox = Gtk2::HBox->new(0);
	$hbox->pack_start(my $lbox = Gtk2::HBox->new(0) ,1,1,0); my $L = sub{$lbox->pack_start(pop,0,0,0)};
	$hbox->pack_start(my $rbox = Gtk2::HBox->new(0) ,0,0,0); my $R = sub{$rbox->pack_start(pop,0,0,0)};
	{
		&$L(my $btn = Gtk2::ToggleButton->new_with_label(ThereMaxi::Device->name) );
		$btn->set_tooltip_markup('Sync/Async Operation Mode');
		$btn->set_active(0);
		$btn->set_sensitive(0);
		$btn->get_child->modify_fg('insensitive',Gtk2::Gdk::Color->parse('red'));
		$btn->get_child->modify_fg('normal',Gtk2::Gdk::Color->parse('green'));
		$btn->modify_bg('active',Gtk2::Gdk::Color->parse('orange'));
		$btn->signal_connect(toggled=>sub{ ThereMaxi::Device->sync_on_change($btn->get_active) });
		ThereMaxi::Event->connect('device/discover'=>sub{ $btn->set_sensitive(pop); $btn->set_active(0) ;$btn->get_child->set_text(ThereMaxi::Device->name) });
	}{
		&$L(my $btn = Gtk2::ToolButton->new(undef,undef) );
		my $action; $btn->signal_connect(clicked=>sub{ &$action if $action });
		my $auto = 0;
		my $disc = 0;
		my $setup = sub
		{
			$btn->set_sensitive(!$auto);
			$btn->set_stock_id( $disc ? 'gtk-connect' : 'gtk-disconnect' );
			if ( $auto )
			{
				$btn->set_tooltip_markup('Disabled due to automatic Discovery');
				$action = undef;
			}
			elsif ( $disc )
			{
				$btn->set_tooltip_markup('Disconnect from Theremini');
				$action = \&ThereMaxi::Device::offline;
			}
			else
			{
				$btn->set_tooltip_markup('Try to discover Theremini');
				$action = \&ThereMaxi::Device::discover;
			}
		};
		ThereMaxi::Event->connect
		(
			'prefs/discovery_interval' => sub{ $auto = pop; &$setup },
			'device/discover'          => sub{ $disc = pop; &$setup },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Send') );
		$btn->set_tooltip_markup("Send current Preset to selected Preset on Theremini\n(without saving)");
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Device::send_preset);
		my $disc = 0;
		my $sync = 1;
		ThereMaxi::Event->connect
		(
			'device/discover'       => sub{ $disc = pop; $btn->set_sensitive($disc&&!$sync) },
			'device/sync_on_change' => sub{ $sync = pop; $btn->set_sensitive($disc&&!$sync) },
		);
	}
	&$L( Gtk2::SeparatorToolItem->new );
	{
		&$L(my $btn = Gtk2::Button->new('Sync') );
		$btn->set_tooltip_markup('Load all Presets from Theremini');
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Device::load_presets);
		ThereMaxi::Event->connect('device/discover'=>sub{ $btn->set_sensitive(pop) });
	}{
		&$L(my $btn = Gtk2::Button->new('QuickSave') );
		$btn->set_tooltip_markup('Save current Preset to selected Preset on Theremini');
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Preset::save);
		ThereMaxi::Event->connect('device/discover'=>sub{ $btn->set_sensitive(pop) });
	}{
		&$L(my $btn = Gtk2::Button->new('Store') );
		$btn->set_tooltip_markup('Save current Preset locally');
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Preset::save_local);
		my $sens = 0;
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $sens = 0; $btn->set_sensitive(0) },
			'switch/presets' => sub{ $sens = 1; $btn->set_sensitive(0) },
			'preset/changed' => sub{ $btn->set_sensitive(pop) if $sens },
		);
	}
	&$L( Gtk2::SeparatorToolItem->new );
	{
		&$L(my $btn = Gtk2::Button->new('Copy') );
		$btn->set_tooltip_markup('Copy current Preset to selected Library');
		$btn->set_sensitive(0);
		my $dst;
		$btn->signal_connect(clicked=>sub{ ThereMaxi::Library->copy_preset($dst) });
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $btn->set_sensitive(0); $dst = defined($_[2]) ? undef : $_[1] },
			'switch/presets' => sub{ $btn->set_sensitive( $dst ? 1 : 0 ) },
		);
	}
	&$L( Gtk2::Label->new('Library:') );
	{
		&$L(my $btn = Gtk2::Button->new('New') );
		$btn->set_tooltip_markup('Create new empty Library');
		$btn->signal_connect(clicked=>\&ThereMaxi::Editor::Library::Manager::new_library);
	}{
		&$L(my $btn = Gtk2::Button->new('Preset') );
		$btn->set_tooltip_markup('Create new Preset in selected Library');
		$btn->set_sensitive(0);
		my $dst;
		$btn->signal_connect(clicked=>sub{ $_[0]->ThereMaxi::Editor::Library::Manager::new_preset($dst) if defined $dst });
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $dst = $_[1]; $btn->set_sensitive( defined($_[1]) && !defined($_[2]) ) },
			'switch/presets' => sub{ $dst = undef; $btn->set_sensitive(0) },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Save'));
		$btn->set_tooltip_markup('Save current Preset to selected Library');
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Preset::save_local);
		my $sens = 0;
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $sens = defined $_[2]; $btn->set_sensitive(0) },
			'switch/presets' => sub{ $sens = 0; $btn->set_sensitive(0) },
			'preset/changed' => sub{ $btn->set_sensitive(pop) if $sens },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Remove') );
		$btn->set_tooltip_markup('Remove selected Preset from current Library');
		$btn->set_sensitive(0);
		$btn->signal_connect(clicked=>\&ThereMaxi::Library::drop_preset);
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $btn->set_sensitive( defined($_[1]) && defined($_[2]) ) },
			'switch/presets' => sub{ $btn->set_sensitive(0) },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Rename') );
		$btn->set_tooltip_markup('Rename current Library');
		$btn->set_sensitive(0);
		my $dst;
		$btn->signal_connect(clicked=>sub{ $_[0]->ThereMaxi::Editor::Library::Manager::rename_library($dst) if defined $dst  });
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $dst = $_[1]; $btn->set_sensitive( defined($_[1]) && !defined($_[2]) ) },
			'switch/presets' => sub{ $dst = undef; $btn->set_sensitive(0) },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Delete') );
		$btn->set_tooltip_markup('Delete current Library');
		$btn->set_sensitive(0);
		my $dst;
		$btn->signal_connect(clicked=>sub{ ThereMaxi::Library->drop($dst) if defined $dst });
		ThereMaxi::Event->connect
		(
			'switch/library' => sub{ $dst = $_[1]; $btn->set_sensitive( defined($_[1]) && !defined($_[2]) ) },
			'switch/presets' => sub{ $dst = undef; $btn->set_sensitive(0) },
		);
	}{
		&$L(my $btn = Gtk2::Button->new('Import') );
		$btn->set_tooltip_markup('Import Library from .theremini Files');
		$btn->signal_connect(clicked=>\&ThereMaxi::Editor::Library::Manager::import_library);
	}
	&$R( Gtk2::SeparatorToolItem->new );
	{
		&$R(my $btn = Gtk2::ToolButton->new_from_stock('gtk-preferences') );
		$btn->set_tooltip_markup('Manage '.$main::NAME.' Preferences');
		$btn->signal_connect(clicked=>\&ThereMaxi::Editor::Prefs::window);
	}{
		&$R(my $btn = Gtk2::ToolButton->new_from_stock('gtk-quit') );
		$btn->set_tooltip_markup('Quit '.$main::NAME);
		$btn->signal_connect(clicked=>sub{ $btn->get_toplevel->event(Gtk2::Gdk::Event->new('delete')) });
	}
	$hbox;
}


1;
