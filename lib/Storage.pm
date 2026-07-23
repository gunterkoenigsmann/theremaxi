
package ThereMaxi::Storage;

use strict;
use warnings;

use JSON::PP;


sub load
{
	my(undef,$file) = @_;
	my $data = [];
	if ( open my$F, "<$file" )
	{
		local $/;
		$data = decode_json <$F>;
		close $F;
	}
	return $data unless wantarray;
	return @$data if 'ARRAY' eq ref $data;
	return %$data if 'HASH' eq ref $data;
	$data;
}


sub save
{
	my(undef,$file,$data) = @_;
	open my$F, ">$file" or die "$file: $!";
	print $F JSON::PP->new->utf8->pretty->encode($data);
	close $F;
}


1;
