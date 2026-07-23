
package ThereMaxi::Library;

use strict;
use warnings;

use MIME::Base64;


my %LIBRARY;

sub init
{
	%LIBRARY = map {&_name_($_)=>scalar ThereMaxi::Storage->load($_)} glob $main::STATE{editor}->{library_path}.qq("/*.theremaxi");
	ThereMaxi::Event->fire('sync/library');

	if ( -r (my$file=$main::STATE{editor}->{library_path}.'/.theremaxi') )
	{
		$LIBRARY{'.'} = scalar ThereMaxi::Storage->load($file);
		ThereMaxi::Event->fire('sync/presets');
	}
}


sub list_library { sort grep !/^\./, keys %LIBRARY }
sub list_presets { map {$_->{_ps}} @{$LIBRARY{$_[1]}} }
sub load_preset  { map {$_=>$LIBRARY{$_[1]}->[$_[2]]->{$_}} keys %{$LIBRARY{$_[1]}->[$_[2]]} } # poor mans clone !


sub new
{
	my(undef,$run,$lib) = @_;
	return 0 if exists $LIBRARY{$lib};
	return 0 if -e $main::STATE{editor}->{library_path}."/$lib.theremaxi";
	return 1 unless $run;
	$LIBRARY{$main::STATE{preset}=$lib} = [];
	ThereMaxi::Event->fire('sync/library');
	1;
}


sub new_preset
{
	my(undef,$lib,$preset) = @_;
	my $nr = @{$LIBRARY{$lib}};
	$LIBRARY{$lib}->[$nr] =
	{
		'_nr' => $nr,
		'_ps' => $preset,
	};
	$main::STATE{preset} = "$lib/$nr";
	&save(undef,$lib);
}


sub save
{
	my(undef,$lib,$data) = @_;
	$LIBRARY{$lib} = $data if ref $data;
	ThereMaxi::Storage->save($main::STATE{editor}->{library_path}.'/'.( $lib eq '.' ? '' : $lib ).'.theremaxi',$LIBRARY{$lib});
	ThereMaxi::Event->fire('sync/'.( $lib eq '.' ? 'presets' : 'library' ));
	1;
}


sub save_preset
{
	my(undef,$lib,$nr,%values) = @_;
	%values = ThereMaxi::Preset->get_values unless %values;
	while ( my($CC,$value) = each %values )
	{
		$LIBRARY{$lib}->[$nr]->{$CC} = $value;
	}
	&save(undef,$lib);
}


sub drop
{
	my(undef,$lib) = @_;
	delete $LIBRARY{$lib};
	$lib = $main::STATE{editor}->{library_path}."/$lib.theremaxi";
	unlink $lib if -e $lib;
	$main::STATE{preset} = undef;
	ThereMaxi::Event->fire('sync/library');
	ThereMaxi::Event->fire('switch/library',undef,0);
	1;
}


sub drop_preset
{
	my(undef,$lib,$nr) = @_;
	($lib,$nr) = split '/', $main::STATE{preset} unless defined $lib;
	die 'Library=.' if $lib eq '.';
	&save(undef,$main::STATE{preset}=$lib,&_renumber_( grep { $_->{_nr} ne $nr } @{$LIBRARY{$lib}} ));
}


sub copy_preset
{
	my(undef,$lib) = @_;
	die 'Library=.' if $lib eq '.';
	my %values = ThereMaxi::Preset->get_values;
	$values{_nr} = my $nr = @{$LIBRARY{$lib}};
	$LIBRARY{$lib}->[$nr] = \%values;
	$main::STATE{preset} = "$lib/$nr";
	&save(undef,$lib);
}


sub rename
{
	my(undef,$run,$old,$new) = @_;
	return 0 unless exists $LIBRARY{$old};
	return 0 if     exists $LIBRARY{$new};
	return 0 if     -e $main::STATE{editor}->{library_path}."/$new.theremaxi";
	return 1 unless $run;
	if ( -e $main::STATE{editor}->{library_path}."/$old.theremaxi" )
	{
		return 0 unless rename
			$main::STATE{editor}->{library_path}."/$old.theremaxi",
			$main::STATE{editor}->{library_path}."/$new.theremaxi";
	}
	$LIBRARY{$main::STATE{preset}=$new} = delete $LIBRARY{$old};
	ThereMaxi::Event->fire('sync/library');
	1;
}


sub _renumber_
{
	my $nr = 0;
	[ map { $_->{_nr} = $nr++ ;$_ } @_ ];
}


sub import_xml
{
	my(undef,$src) = @_;
	ThereMaxi::Event->catch;
	eval
	{
		local $SIG{__DIE__};
		ThereMaxi::Editor->status("Import: $src");
		my $lib = &_name_($src);
		open my$I, "<$src" or die "$src: $!";
		my @data;
		while (<$I>)
		{
			next unless /data="([^"]+)"/; #"
			push @data, ThereMaxi::Preset->import_data(scalar@data, pack 'v*', unpack 'n*', decode_base64($1));
		}
		close $I;
		&save(undef,$lib,\@data);
		ThereMaxi::Editor->status("Import successful: $lib");
	};
	ThereMaxi::Event->release;
	ThereMaxi::Event->fire('ERROR',$@) if $@;
}


sub _name_
{
	my($name) = @_;
	$name =~ m'^(.*)/([^/]+)\.([^.]+)$' or die "NAME=$name";
	$2; #{dir=>$1,name=>$2,ext=>$3}
}


1;
