use Getopt::Std;
use File::Basename;
my $PROGNAME = "GPREFIX_cp";

my $trace = 1;
my $mode = $ENV{'GPREFIX_MODE'};

if (!$mode) {
    $mode = "LOCAL";
}

my %options=();
getopts("hv",\%options);
@_ = @ARGV;

if ($options{h}) {
    print STDERR "usage: $PROGNAME <src> <dst>\n";
    exit(0);
}

my $src;
my $dst; 

my $verbose;
if ($options{v}) {
    $verbose = "-v";
}

$src->{path} = shift(@_);
$dst->{path} = shift(@_);

my ($sp,$sse) = split('@',"$src->{path}");
my ($dp,$dse) = split('@',"$dst->{path}");

if ($sse) {
    $src->{se} = $sse;
    $src->{path} = $sp;
} else {
    $src->{se} = "";
}


if ($dse) {
    $dst->{se} = $dse;
    $dst->{path} = $dp;
} else {
    $dst->{se} = "";
}


print "$src->{path} [$src->{se}] $dst->{path} [$dst->{se}]\n";
if ( ($src->{path} eq "") ) {
    print STDERR "error: ($PROGNAME) you have to specify the source path!\n";
    exit(-1);
}

if ( ($dst->{path} eq "") ) {
    print STDERR "error: ($PROGNAME) you have to specify the destination path!\n";
    exit(-1);
}


$trace and print "Copying $src->{path} => $dst->{path}\n";

$src->{name} = $src->{path};
$dst->{name} = $dst->{path};

if ($src->{path} =~ /file:/) {
    $src->{name} =~ s/file\://g;
    $src->{mode} = "LOCAL";
} else {
    if ($src->{path} =~ /GPREFIX:/) {
	$src->{name} =~ s/GPREFIX://g;
	$src->{mode} = "GRID";	
    } else {
	if ($mode eq "LOCAL") {
	    $src->{name} = $src->{path};
	    $src->{mode} = "LOCAL";
	    
	} else {
	    if ($mode eq "GRID") {
		$src->{name} = $src->{path};
		$src->{mode} = "GRID";
	    } else {
		print "${PROGNAME}: illegal mode set - exit!\n";
		exit(-1);
	    }
	}
    }
}

if ($dst->{path} =~ /file:/) {
    $dst->{name} =~ s/file\://g;
    $dst->{mode} = "LOCAL";
} else {
    if ($dst->{path} =~ /GPREFIX:/) {
	$dst->{name} =~ s/GPREFIX://g;
	$dst->{mode} = "GRID";
    } else {
	if ($mode eq "LOCAL") {
	    $dst->{name} = $dst->{path};
	    $dst->{mode} = "LOCAL";
	} else {
	    if ($mode eq "GRID") {
		$dst->{name} = $dst->{path};
		$dst->{mode} = "GRID";
	    } else {
		print STDERR  "${PROGNAME}: illegal mode set - exit!\n";
		exit(-1);
	    }
	}
    }
}

if ($src->{mode} eq "LOCAL") {
    if (!($src->{name} =~ /^\// )) {
	$src->{name} = "$ENV{'PWD'}/$src->{name}";
    }
}

if ($src->{mode} eq "GRID") {
    if (!($src->{name} =~ /^\//)) {
	$src->{name} = "$ENV{'GPREFIX_PWD'}/$src->{name}";
    }
}

if ($dst->{mode} eq "LOCAL") {
    if (!($dst->{name} =~ /^\//)) {
	$dst->{name} = "$ENV{'PWD'}/$dst->{name}";
    }
}

if ($dst->{mode} eq "GRID") {
    if (!($dst->{name} =~ /^\//)) {
	$dst->{name} = "$ENV{'GPREFIX_PWD'}/$dst->{name}";
    }
}

$trace and print "Copying $src->{name} [$src->{mode}] => $dst->{name} [$dst->{mode}]\n";

my $rsrcpath;
my $rdstpath;

my $srcok=0;
my $dstok=0;

##############################################################################
# src checking
##############################################################################
if ($src->{mode} eq "GRID") {
    # get the access envelope for this file to read
    $rsrcpath = "/tmp/.__env__" . rand() . rand();
    open ENV, "gbbox -o url,envelope access read $src->{name} $src->{se} 2>&1 |";
    open ENVOUT, ">$rsrcpath";
    chmod 0600, $rsrcpath;
    my $first = 1;
    while (<ENV>) {
	if ($_ =~ /SOAP\sFAULT/) {
	    printf STDERR "error: ($PROGNAME) cannot talk to apiservice - you may need to reconnect\n";
	    $srcok=0;
	    last;
	}
	if ($first) {
	    my ($url,$rest) = split ("   ",$_,2);
	    $src->{turl} = $url;
	    print ENVOUT "$rest";
	    $first = 0;
	} else {
	    $srcok=1;
	    if (! ($_ =~ /^\ \ \ /)) {
		print ENVOUT "$_";
	    }
	}
    }

    close ENVOUT;
    close ENV;
}

if ($src->{mode} eq "LOCAL") {
    if ( -r $src->{name}) {
	$src->{turl} = $src->{name};
	$srcok=1;
    }
}

$dst->{envelope} = "";

##############################################################################
# dst checking
##############################################################################
if ($dst->{mode} eq "GRID") {
    # get the access envelope for this file to write
    $rdstpath = "/tmp/.__env__" . rand() . rand();
    open ENV, "gbbox -o url,envelope access write-once $dst->{name} $dst->{se} 2>&1 |";
    open ENVOUT, ">$rdstpath";
    chmod 0600, $rdstpath;
    my $first=1;
    while (<ENV>) {
	if ($_ =~ /SOAP\sFAULT/) {
	    printf STDERR "error: ($PROGNAME) cannot talk to apiservice - you may need to reconnect\n";
	    $srcok=0;
	    last;
	}
	if ($first) {
	    my ($url,$rest) = split ("   ",$_);
	    $dst->{turl} = $url;
	    print ENVOUT "$rest";
	    $dst->{envelope} .= "$rest";
	    $first = 0;
	} else {
	    $dstok=1;
	    if (! ($_ =~ /^\ \ \ /)) {
		print ENVOUT "$_";
		$dst->{envelope} .= "$_";
	    }
	}
    }

    close ENVOUT;
    close ENV;
}

if ($dst->{mode} eq "LOCAL") {
    my $basedir = dirname $dst->{name};
    if ( -e $dst->{name}) {
	if ( -w $dst->{name}) {
	    $dst->{turl} = $dst->{name};
	    $dstok=1;
	}
    } else {
	if ( -x $basedir ) {
	    $dst->{turl} = $dst->{name};
	    $dstok=1;
	}
    }
}

if ($srcok && $dstok) {
    # copy ths stuff

    ###########################################################################
    
    # use xdrcp now
    ###########################################################################
    $trace and print "XCopy $src->{turl} => $dst->{turl}\n";
    my $failed=0;
    
    if ( ($src->{mode} eq "LOCAL") && ($dst->{mode} eq "GRID") ) {
	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	    $atime,$mtime,$ctime,$blksize,$blocks)
	    = stat($src->{turl} );
	$dst->{size} = $size;
	$command = "xcp $src->{turl} $dst->{turl} -authz $rdstpath $verbose";
	if (!run_command($command)) {
	    printf STDERR "error: ($PROGNAME) copy command failed\n";
	    $failed=1;
	}
    }
    
    if ( ($src->{mode} eq "GRID") && ($dst->{mode} eq "LOCAL") ) {
	$command = "xcp $src->{turl} $dst->{turl} -authz $rsrcpath $verbose";
	if (!run_command($command)) {
	    printf STDERR "error: ($PROGNAME) copy command failed\n";
	    $failed=1;
	} 
	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	    $atime,$mtime,$ctime,$blksize,$blocks)
	    = stat($dst->{turl} );
	$dst->{size} = $size;
    }
    
    if ( ($src->{mode} eq "LOCAL") && ($dst->{mode} eq "LOCAL") ) {
	$command = "cp $src->{turl} $dst->{turl}"; 
	if (!run_command($command)) {
	    printf STDERR "error: ($PROGNAME) copy command failed\n";
	    $failed=1;
	}
	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	    $atime,$mtime,$ctime,$blksize,$blocks)
	    = stat($dst->{turl} );
	$dst->{size} = $size;
    }
    
    if ( ($src->{mode} eq "GRID") && ($dst->{mode} eq "GRID") ) {
	$tmppath = "/tmp/.__env__" . rand() . rand();    
	$command = "xcp $src->{turl} $tmppath -authz $rsrcpath $verbose";
	if (!run_command($command)) {
	    printf STDERR "error: ($PROGNAME) copy command failed\n";
	    $failed=1;
	} else {
	    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		$atime,$mtime,$ctime,$blksize,$blocks)
		= stat($tmppath);
	    $dst->{size} = $size;
	    $command = "xcp $tmppath $dst->{turl} -authz $rdstpath $verbose";
	    if (!run_command($command)) {
		printf STDERR "error: ($PROGNAME) copy command failed\n";
		$failed=1;
	    }
	}
    }
    
    if (!$failed) {
	# commit the destination envelope, if it was on the grid
	if ($dst->{mode} eq "GRID") {
	    open COMMIT , "export GCLIENT_EXTRA_ARG=\"$dst->{envelope}\"; gbbox -o _errorval_ commit $dst->{size} 2>&1 |";
	    while (<COMMIT>) {
		if (!($_ =~ /ERROR/)) {
		    print STDERR "error: ($PROGNAME) could not enter the lfn into the catalogue: $_!\n";
		    $failed=1;
		}
	    }
	    close COMMIT;
	}
    }
    
    ($tmppath ne "") and unlink $tmppath;
#    ($rsrcpath ne "") and unlink $rsrcpath;
#    ($rdstpath ne "") and unlink $rdstpath;
    
} else {
    if (!$srcok) {
	printf STDERR "error: ($PROGNAME) cannot access source: $src->{name}\n";
    }
    if (!$dstok) {
	printf STDERR "error: ($PROGNAME) cannot access destination: $dst->{name}\n";
    }
}

if ($failed) {
    exit(-1);
} else {
    exit(0);
}

sub run_command() {

    my $command = shift;
    my $result = 1;
    open CMD , "$command 2>&1 |";
    while (<CMD>) {
	if ($_ =~ /^Error/) {
	    $result = 0;
	    print "====> ",$_;
	} else {
	    print $_;
	}
    }

    close CMD;
    return $result;
}
