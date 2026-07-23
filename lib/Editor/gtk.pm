
use strict;
use warnings;

use Gtk2::Gdk::Keysyms;


package Gtk2::Label;

sub new_with_markup
{
	my(undef,@args) = @_;
	my $label = Gtk2::Label->new;
	$label->set_markup(join '', @args);
	$label;
}


package Gtk2::Alignment;

sub new_with_object
{
	my(undef,$object,@args) = @_;
	my $align = Gtk2::Alignment->new(@args);
	$align->add($object);
	$align;
}


package Gtk2::FileFilter;

sub new_with_pattern
{
	my(undef,%args) = @_;
	my $filter = Gtk2::FileFilter->new;
	while ( my($name,$pattern) = each %args )
	{
		$filter->set_name($name);
		$filter->add_pattern($_) for @$pattern;
	}
	$filter;
}


package Gtk2::EventBox;

sub new_with_object
{
	my(undef,$object) = @_;
	my $box = Gtk2::EventBox->new;
	$box->add($object);
	$box;
}


package  Gtk2::Frame;

sub new_with_object
{
	my(undef,$object,$label) = @_;
	my $frame = Gtk2::Frame->new;
	if ( $label )
	{
		ref($label)
			? $frame->set_label_widget($label)
			: $frame->set_label($label)
		;
	}
	$frame->add($object);
	$frame;
}


package Gtk2::ScrolledWindow;

sub new_with_object
{
	my(undef,$object,@policy) = @_;
	my $scroll = Gtk2::ScrolledWindow->new;
	$scroll->add($object);
	$scroll->set_policy(@policy);
	$scroll;
}

sub new_with_viewport
{
	my(undef,$object,@policy) = @_;
	my $scroll = Gtk2::ScrolledWindow->new;
	$scroll->add_with_viewport($object);
	$scroll->set_policy(@policy);
	$scroll;
}


1;
