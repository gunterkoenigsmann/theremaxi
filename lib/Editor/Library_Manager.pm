
package ThereMaxi::Editor::Library::Manager;

use strict;
use warnings;


sub new_library
{
	my($parent) = @_;

	my $dlg = Gtk2::Dialog->new('New Library',$parent->get_toplevel,'modal',
		'gtk-cancel' => 'cancel',
		'gtk-save'   => 'accept',
	);

	my $name = Gtk2::Entry->new;
	$name->signal_connect(changed=>sub
	{
		my $value = $name->get_text;
		my $len = length $value;
		$value =~ s/\s+$//g;
		$value =~ s/^\s+//g;
		$name->set_text($value);
		$name->set_position(-1) if length $value < $len;
		$dlg->set_response_sensitive('accept',ThereMaxi::Library->new(0,$value));
	});
	$name->set_text('New Library');

	my $hbox = Gtk2::HBox->new;
	$hbox->add($name);

	$dlg->vbox->pack_start($hbox,0,0,0);
	$dlg->show_all;

	ThereMaxi::Library->new(1,$name->get_text) if $dlg->run eq 'accept';

	$dlg->destroy;
}


sub rename_library
{
	my($parent,$lib) = @_;

	my $dlg = Gtk2::Dialog->new('Rename Library',$parent->get_toplevel,'modal',
		'gtk-cancel' => 'cancel',
		'gtk-save'   => 'accept',
	);
	$dlg->set_response_sensitive('accept',0);

	my $name = Gtk2::Entry->new;
	$name->signal_connect(changed=>sub
	{
		my $value = $name->get_text;
		my $len = length $value;
		$value =~ s/\s+$//g;
		$value =~ s/^\s+//g;
		$name->set_text($value);
		$name->set_position(-1) if length $value < $len;
		$dlg->set_response_sensitive('accept',ThereMaxi::Library->rename(0,$lib,$value));
	});
	$name->set_text($lib);

	my $hbox = Gtk2::HBox->new;
	$hbox->add($name);

	$dlg->vbox->pack_start($hbox,0,0,0);
	$dlg->show_all;

	ThereMaxi::Library->rename(1,$lib,$name->get_text) if $dlg->run eq 'accept';

	$dlg->destroy;
}


sub new_preset
{
	my($parent,$lib) = @_;

	my $dlg = Gtk2::Dialog->new('New Preset',$parent->get_toplevel,'modal',
		'gtk-cancel' => 'cancel',
		'gtk-save'   => 'accept',
	);

	my $name = Gtk2::Entry->new;
	$name->set_max_length(13);
	$name->signal_connect(changed=>sub
	{
		my $value = uc $name->get_text;
		my $len = length $value;
		$value =~ s/[^ -_]//g;
		$value =~ s/\s+$//g;
		$value =~ s/^\s+//g;
		$name->set_text($value);
		$name->set_position(-1) if length $value < $len;
		$dlg->set_response_sensitive('accept',length $value);
	});
	$name->set_text('NEW PRESET');

	my $hbox = Gtk2::HBox->new;
	$hbox->add(Gtk2::Label->new($lib));
	$hbox->add($name);

	$dlg->vbox->pack_start($hbox,0,0,0);
	$dlg->show_all;

	ThereMaxi::Library->new_preset($lib,$name->get_text) if $dlg->run eq 'accept';

	$dlg->destroy;
}


sub import_library
{
	my $dlg = Gtk2::FileChooserDialog->new('Import Library',$_[0]->get_toplevel,'open',
		'gtk-cancel' => 'cancel',
		'gtk-open'   => 'accept',
	);
	$dlg->add_filter( Gtk2::FileFilter->new_with_pattern('*.theremini'=>['*.theremini']) );
	$dlg->set_select_multiple(1);
	$dlg->set_show_hidden(0);
	if ( $dlg->run eq 'accept' )
	{
		ThereMaxi::Library->import_xml($_) for $dlg->get_filenames;
	}
	$dlg->destroy;
}


1;
