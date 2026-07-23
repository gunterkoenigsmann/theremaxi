
package ThereMaxi::Controller::_ps;

use strict;
use warnings;

use base 'ThereMaxi::Controller::name';


sub widget
{
	my($self) = @_;
	my $text = '';;

	my $box = Gtk2::HBox->new(0);
	$box->pack_start(ThereMaxi::Controller->new('_nr')->widget,0,0,0);

	my $name = $self->{__WIDGET__} = Gtk2::Entry->new;
	$box->pack_start($name,1,1,0);

	my $btn = Gtk2::ToolButton->new_from_stock('gtk-ok');
	$box->pack_start($btn,0,0,0);

	delete $self->{LAYOUT}->{LabelSizeGroup} if $self->{LAYOUT}->{LabelSizeGroup};
	$self->{Alignment} = [0,0.5];
	my $widget = $self->SUPER::widget($box,sub{ $name->set_text($text=uc$_[0]||'') });

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
		return $btn->set_sensitive(0) unless $self->value_compare($text,$value);
		$btn->set_sensitive(length $value);
		$self->value_changed($value);
	});
	$name->signal_connect(focus_out_event=>sub{ $name->set_text($text); 0});
	$name->signal_connect(key_release_event=>sub{ $name->set_text($text) if $_[1]->keyval == $Gtk2::Gdk::Keysyms{Escape} });

	$btn->set_tooltip_markup('Rename selected Preset');
	$btn->set_sensitive(0);
	$btn->signal_connect(clicked=>sub
	{
		$btn->set_sensitive(0);
		ThereMaxi::Preset->save('_ps');
	});

	ThereMaxi::Event->connect('preset/loaded'=>sub
	{
		my(undef,$lib,$nr) = @_;
		$self->{__LABEL__}->set_markup( '<b>'.( $lib eq '.' ? 'Theremini Presets' : $lib ).'</b>' );
		$btn->set_sensitive(0);
	});

	$widget;
}


1;
