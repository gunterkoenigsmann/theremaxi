
package ThereMaxi::Controller;

use strict;
use warnings;

require "$main::LIB/Controller/numeric.pm";
require "$main::LIB/Controller/prozent.pm";
require "$main::LIB/Controller/choice.pm";
require "$main::LIB/Controller/name.pm";


# our, not my: tools/dump-protocol.pl reads this table to generate the machine
# readable description of the protocol. It is the authoritative copy.
our %CONTROLLER =
(
	_nr => { name=>'Preset Number'                                                                    ,preset=>1,                                                        props=>{ sync_on_change=>undef }},
	_ps => { name=>'Preset Name'            ,show=>[ 1,'Main'    , undef          ,'Preset Name'     ],preset=>1,                                                        props=>{ sync_on_change=>undef }},
	103 => { name=>'Preset Volume'          ,show=>[ 2,'Main'    , undef          ,'Preset Volume'   ],preset=>1, typ=>'prozent',                                        props=>{ MidiFeedbackLoop=>undef }},
	 85 => { name=>'Scale'                  ,show=>[ 3,'Basic'   ,'Notes'         ,'Scale'           ],preset=>1,                                                        props=>{ MidiFeedbackLoop=>undef }},
	 86 => { name=>'Root Note'              ,show=>[ 4,'Basic'   ,'Notes'         ,'Root'            ],preset=>1,                                                        props=>{ MidiFeedbackLoop=>undef }},
	 84 => { name=>'Pitch Correction Amount',show=>[ 5,'Basic'   ,'Notes'         ,'Pitch Correction'],preset=>1, typ=>'prozent'                                         },
	 90 => { name=>'Wave Selection'         ,show=>[ 6,'Basic'   ,'Waves'         ,'Waveform'        ],preset=>1,                                                        props=>{ MidiFeedbackLoop=>undef }},
	  9 => { name=>'Wavetable Scan Rate'    ,show=>[ 7,'Basic'   ,'Waves'         ,'Scan Rate'       ],preset=>1, typ=>'_090dep', max=>32, dig=>2, fmt=>'%2.2f Hz'       },
	 20 => { name=>'Scan Amount'            ,show=>[ 8,'Basic'   ,'Waves'         ,'Scan Amount'     ],preset=>1, typ=>'_090dep', max=>2,  dig=>2, fmt=>'%1.2f'          },
	 21 => { name=>'Scan Position'          ,show=>[ 9,'Basic'   ,'Waves'         ,'Scan Position'   ],preset=>1, typ=>'_090dep', max=>2,  dig=>2, fmt=>'%1.2f'          },
	 80 => { name=>'Filter Type'            ,show=>[10,'Basic'   ,'Filter'        ,'Filter Type'     ],preset=>1,                                                        props=>{ MidiFeedbackLoop=>undef }},
	 74 => { name=>'Filter Cutoff Freq'     ,show=>[11,'Basic'   ,'Filter'        ,'Cutoff'          ],preset=>1                                                         },
	 71 => { name=>'Filter Resonance'       ,show=>[12,'Basic'   ,'Filter'        ,'Filter Resonance'],preset=>1, typ=>'prozent'                                         },
	 29 => { name=>'Filter Pitch Tracking'  ,show=>[13,'Basic'   ,'Filter'        ,'Pitch Track'     ],preset=>1, typ=>'prozent', min=>-800, max=>800                    },
	_fx => { name=>'Effect Name'            ,show=>[14,'Basic'   ,'Effect'        ,'Delay Type'      ],preset=>1,                                                        props=>{ MidiFeedbackLoop=>undef }},
	 12 => { name=>'Delay Time'             ,show=>[15,'Basic'   ,'Effect'        ,'Delay Time'      ],preset=>1, typ=>'numeric', max=>836, fmt=>'%d ms'                 },
	 14 => { name=>'Delay Feedback'         ,show=>[16,'Basic'   ,'Effect'        ,'Delay Feedback'  ],preset=>1, typ=>'prozent',                                        },
	 91 => { name=>'Effect Mix'             ,show=>[17,'Basic'   ,'Effect'        ,'Delay Amount'    ],preset=>1, typ=>'prozent'                                         },
	 26 => { name=>'Vol Mod Volume'         ,show=>[18,'Advanced','Volume Antenna','Volume'          ],preset=>1, typ=>'prozent',            max=>1600                   },
	 25 => { name=>'Vol Mod Scan Amount'    ,show=>[19,'Advanced','Volume Antenna','Wave Scan Amount'],preset=>1, typ=>'prozent', min=>-400, max=>400                    },
	 23 => { name=>'Vol Mod Scan Freq.'     ,show=>[20,'Advanced','Volume Antenna','Wave Scan Rate'  ],preset=>1, typ=>'prozent', min=>-400, max=>400                    },
	 27 => { name=>'Vol Mod Cutoff'         ,show=>[21,'Advanced','Volume Antenna','Filter Cutoff'   ],preset=>1, typ=>'prozent', min=>-100, max=>100                    },
	 28 => { name=>'Vol Mod Resonance'      ,show=>[22,'Advanced','Volume Antenna','Filter Resonance'],preset=>1, typ=>'prozent', min=>-200, max=>200                    },
	 24 => { name=>'Pitch Mod Scan Amount'  ,show=>[23,'Advanced','Pitch Antenna' ,'Wave Scan Amount'],preset=>1, typ=>'prozent', min=>-400, max=>400                    },
	 22 => { name=>'Pitch Mod Scan Freq.'   ,show=>[24,'Advanced','Pitch Antenna' ,'Wave Scan Rate'  ],preset=>1, typ=>'prozent', min=>-400, max=>400                    },
	 30 => { name=>'Pitch Mod Resonance'    ,show=>[25,'Advanced','Pitch Antenna' ,'Filter Resonance'],preset=>1, typ=>'prozent', min=>-400, max=>400                    },
	102 => { name=>'Transpose'              ,show=>[26,'Advanced', undef          ,'Pitch Transpose' ],preset=>1, typ=>'numeric', min=>-64, max=>63, fmt=>'%d Semitones' },
	 87 => { name=>'Lowest MIDI Note'       ,show=>[27,'Global'  , undef          ,'Low Note'        ]          , typ=>'_notes' ,                                        props=>{ MidiFeedbackLoop=>undef }},
	 88 => { name=>'Highest MIDI Note'      ,show=>[28,'Global'  , undef          ,'High Note'       ]          , typ=>'_notes' ,                                        props=>{ MidiFeedbackLoop=>undef }},
	  7 => { name=>'Master Volume'          ,show=>[29,'Global'  , undef          ,'Master Volume'   ]          , typ=>'prozent',                                        props=>{ MidiFeedbackLoop=>undef }},
);
#	119 : Save To Current Preset -> Device.pm


my %PROPS =
(
	sync_on_change => 1,
	MidiFeedbackLoop => 1,
);


sub new
{
	my $base = shift;
	my $CC = shift;
	return $CONTROLLER{$CC}->{__SELF__} if $CONTROLLER{$CC}->{__SELF__};

	my $self = $CC =~ /^\d+$/ ? sprintf '_%03d', $CC : $CC;
	$self = $CONTROLLER{$CC}->{typ} unless -f "$main::LIB/Controller/$self.pm";
	require "$main::LIB/Controller/$self.pm";
	$self = bless {}, $base.'::'.$self; # "$base::$self" interpolates as ${base::} on modern perl

	$CONTROLLER{$CC}->{tunable} = ( $CC =~ /^\d+$/ ) unless defined $CONTROLLER{$CC}->{tunable};

	while ( my($p,$d) = each %PROPS )
	{
		$CONTROLLER{$CC}->{props}->{$p} = defined($main::STATE{controller}->{$p}->{$CC})
			? $main::STATE{controller}->{$p}->{$CC}
			: $d
		unless exists $CONTROLLER{$CC}->{props}->{$p};
	}
	$self->define(CC=>$CC,%{$CONTROLLER{$CC}},@_);

	$CONTROLLER{$CC}->{__SELF__} = $self;
}

sub get {&new(@_)}


sub define
{
	my($self,%self) = @_;
	while ( my($k,$v) = each %self )
	{
		$self->{$k} = $v;
	}
}


sub show_sort { $_[0]->{show}->[0] }
sub show_name { $_[0]->{show}->[3] || $_[0]->{name} }
sub show_path { join ' / ', grep {defined} @{$_[0]->{show}}[1..3] }


sub widget
{
	my($self,$widget,$setter) = @_;

	$self->{set_value} = $setter;
	$self->set_value($main::STATE{controller}->{values}->{$self->{CC}}) unless $self->{preset};

	ThereMaxi::Event->connect
	(
		'controller/set_value'     => sub{ $self->depend(@_) if $_[0] ne $self->{CC} },
		'controller/value_changed' => sub{ $self->depend(@_) if $_[0] ne $self->{CC} },
	)
	if $self->can('depend');

	my $label = $self->{__LABEL__} = Gtk2::Label->new_with_markup('<b>'.$self->show_name.'</b>');
	$label->set_alignment( $self->{Alignment} ? @{$self->{Alignment}} : (0,0.4) );
	$label->modify_fg('normal',Gtk2::Gdk::Color->parse('orange')) if $self->{props}->{sync_on_change};
	$self->{LAYOUT}->{LabelSizeGroup}->add_widget($label) if $self->{LAYOUT}->{LabelSizeGroup};

	my $hbox = Gtk2::HBox->new(0);
	$hbox->pack_start($label,0,0,0);
	$hbox->pack_end($widget,1,1,0);
	$hbox;
}


sub prefs_changed
{
	my($self,$pref,$value) = @_;
	$main::STATE{controller}->{$pref}->{$self->{CC}} = $self->{props}->{$pref} = $value;
	$self->{__LABEL__}->modify_fg('normal',Gtk2::Gdk::Color->parse( $value ? 'orange' : 'black' )) if $pref eq 'sync_on_change';
}


sub set_value
{
	my($self,$value) = @_;
	return if $self->{BLOCKED};
	$self->{VALUE} = $value;
	$self->{set_value}->($value) if 'CODE' eq ref $self->{set_value};
	ThereMaxi::Event->fire('controller/set_value',$self->{CC}=>$value);
}


sub value_changed
{
	my($self,$value) = @_;
	return 0 if $self->{BLOCKED};
	$self->{VALUE} = $value;
	return 0 unless $self->value_compare($self->{CHANGE},$value);
	$self->{CHANGE} = $value;
	$self->ThereMaxi::Device::value_changed if $self->{props}->{sync_on_change};
	$main::STATE{controller}->{values}->{$self->{CC}} = $value unless $self->{preset};
	ThereMaxi::Event->fire('controller/value_changed',$self->{CC}=>$value);
	ThereMaxi::Preset->changes($self->{CC});
	1;
}


sub value_compare
{
	my($self,$a,$b) = @_;
	return 1 unless defined $a;
	return 1 unless defined $b;
	$a ||= 0;
	$b ||= 0;
	$a <=> $b;
}


sub value_import
{
	my($self,$value) = @_;
	$value;
}


sub value_export
{
	my($self,$value) = @_;
	defined($value) ? $value : $self->{VALUE};
}


sub get_value
{
	my($self) = @_;
	$self->{VALUE};
}


sub _list_sort_ { my($a,$b)=@_; &show_sort($CONTROLLER{$a}) <=> &show_sort($CONTROLLER{$b}) }

sub list_tunables  { map {__PACKAGE__->new($_)} sort {&_list_sort_($a,$b)} grep {$CONTROLLER{$_}->{tunable}}        keys %CONTROLLER }
sub list_in_preset { map {__PACKAGE__->new($_)}                            grep {$CONTROLLER{$_}->{preset}}         keys %CONTROLLER }
sub list_with_prop { map {__PACKAGE__->new($_)} sort {&_list_sort_($a,$b)} grep {$CONTROLLER{$_}->{props}->{$_[1]}} keys %CONTROLLER }


1;
