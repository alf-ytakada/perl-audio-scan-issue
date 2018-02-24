use strict;
use warnings;
use 5.010;
use Audio::Scan;

use Data::Dumper;


my $f	= $ARGV[0];

my $info	= Audio::Scan->scan_info($f);

warn Dumper $info;

