########################################################################################
# gapiserver.pl
# - perl configuration frontend to startup the gapiserver binary
# - configuration file is $ENV{HOME}/etc/gapiservice.cfg.xml
########################################################################################

use XML::Simple;
use File::Basename;
use Data::Dumper;
use Net::Domain qw(hostname hostfqdn hostdomain);
my $pid=$$;

if (! defined $ARGV[0] || (($ARGV[0] ne "start") && ($ARGV[0] ne "stop") && ($ARGV[0] ne "status" )) )  {
    print STDERR "Usage: gapiserver.pl <start|status|stop> [<logfile>]\n";
    exit(1);
}

if ( $ARGV[0] eq "stop" ) {

    system("pkill -9 xAlienWorker 2> /dev/null");
    system("pkill -9 xrootd 2> /dev/null");
    system("pkill -9 -f cronolog 2> /dev/null");
    system("pkill -9 -f gapiservermonitor.pl 2> /dev/null");
    exit(0);
}

if ( $ARGV[0] eq "status" ) {
    my $pids= `pgrep xrootd 2> /dev/null`;
    if ($pids eq "") {
        exit(0);
    } else {
        $pids = `pgrep xAlienWorker 2> /dev/null`;
        if ($pids eq "") {
            exit(0);
        }
        exit(10);
    }
}
    
system("pkill -9 xAlienWorker 2> /dev/null");
system("pkill -9 xrootd 2> /dev/null");
system("pkill -9 -f cronolog 2> /dev/null");
system("pkill -9 -f gapiservermonitor.pl 2> /dev/null");

my $logfile = "";
if (defined $ARGV[1]) {
    $logfile = $ARGV[1];
    system("touch $logfile");
    if ( ! -w $logfile ) {
	print STDERR "Error: Cannot write to $logfile\n";
	exit(2);
    }
}

system("mkdir -p $ENV{HOME}/etc/");

if ( ! -w "$ENV{HOME}/etc/" ) {
    print STDERR "Error: cannot access $ENV{HOME}/etc/";
    exit(3);
}


# start a server
print $ARGV[0],$ARGV[1],"\n";
if (! defined $ENV{ALIEN_ROOT}) {
    print STDERR "Warning: ALIEN_ROOT is not defined - trying with /opt/alien ....\n";
    $ENV{ALIEN_ROOT}="/opt/alien";
}


print "$ENV{HOME}/etc/gapiservice.cfg.xml $ENV{ALIEN_ROOT}/api/etc/gapiservice.cfg.xml $ENV{HOME}/etc/gapiservice.cfg.xml\n";
# check if there is a config, if not use the template one and put it into place
if (! -r "$ENV{HOME}/etc/gapiservice.cfg.xml" ) {
    if (open IN, "$ENV{ALIEN_ROOT}/api/etc/gapiservice.cfg.xml") {
	if (open OUT, ">$ENV{HOME}/etc/gapiservice.cfg.xml") {
	    while (<IN>) {
		my $line = $_;
		$line =~ s/HOME/"$ENV{HOME}"/g;
		print $line;
		printf OUT $line;
	    }
	    close IN;
	} else {
	    printf STDERR "Error: cannot create configuration file $ENV{HOME}/etc/gapiservice.xml.cf";
	    exit(4);
	}
	close OUT;
    } else {
	printf STDERR "Error: cannot open template $ENV{HOME}/etc/gapiservice.xml.cf";
	exit(4);
    }
}

my $xsimple = XML::Simple->new();


my $config = $xsimple->XMLin("$ENV{HOME}/etc/gapiservice.cfg.xml",
#				   KeyAttr => {event => 'name', file => 'name', mirror => 'name'},
                                   ForceArray => [ 'arrayvalue'  ],
				   ContentKey => '-content');

my $dumper = new Data::Dumper([$config]);
print $dumper->Dump();

if ( ( ! defined $config->{components}->{component}->{name}) ||
     $config->{components}->{component}->{name} ne "gapiservice" ) {
    print STDERR "Configuration file does not contain the component name 'gapiservice' - fix it!\n";
    exit -1;
}
my $argline="$ENV{ALIEN_ROOT}/api/bin/xrootd ";
my $arglinex="$ENV{ALIEN_ROOT}/api/bin/xAlienWorker ";

my $port=1094;
my $host=Net::Domain::hostname();
my $nworker=64;

defined $config->{components}->{component}->{config}->{param}->{'port'}->{value} and $argline .= "-p $config->{components}->{component}->{config}->{param}->{'port'}->{value} " and $port = $config->{components}->{component}->{config}->{param}->{'port'}->{value};
defined $config->{components}->{component}->{config}->{param}->{'prefork'}->{value} and $nworker = $config->{components}->{component}->{config}->{param}->{'prefork'}->{value};

my $HOST=`hostname -f`;
system("mkdir -p $ENV{HOME}/active");
system("mkdir -p $ENV{HOME}/log");

if ( ! -w "$ENV{HOME}/active/" ) {
    printf STDERR "error: cannot access active pipe directory $ENV{HOME}/active/ - aborting!\n";
    exit(-1);
}

if ( ! -e "$ENV{HOME}/etc/xAlien-auto.cf" ) {
open OUT, ">$ENV{HOME}/etc/xAlien-auto.cf";
print OUT <<EOF;
###########################################################
xrootd.fslib $ENV{ALIEN_ROOT}/api/lib/libXrdxAlienFs.so
xrootd.async off nosf
all.manager $HOST 2131
all.export /dummy /alice/
###########################################################
xrootd.seclib $ENV{ALIEN_ROOT}/api/lib/libXrdSec.so
###########################################################
xalien.outputdir $ENV{HOME}/active/
#xalien.trace all debug
xalien.perlmodule $ENV{ALIEN_ROOT}/api/scripts/gapiserver/AlienAS.pl
xalien.nworker $nworker
sec.protocol $ENV{ALIEN_ROOT}/api/lib/ unix
sec.protocol $ENV{ALIEN_ROOT}/api/lib/ gsi -crl:3 -cert:$ENV{HOME}/.globus/usercert.pem -key:$ENV{HOME}/.globus/userkey.pem -certdir:$ENV{ALIEN_ROOT}/globus/share/certificates/ -gmapopt:0
sec.protbind * gsi unix
EOF
;;
} else {
    print "Warning: $ENV{HOME}/etc/xAlien-auto.cf exists - leaving untouched\n";
}

if ( defined $config->{components}->{component}->{config}->{param}->{'perlmodule'}->{value} ) {
    $config->{components}->{component}->{config}->{param}->{'perlmodule'}->{value} =~ s/\"//g;

    if ( $config->{components}->{component}->{config}->{param}->{'perlmodule'}->{value} =~ /^\//) {
	$arglinex .= "$config->{components}->{component}->{config}->{param}->{'perlmodule'}->{value} ";
    } else {
	$arglinex .= "$ENV{ALIEN_ROOT}/api/scripts/gapiserver/$config->{components}->{component}->{config}->{param}->{'perlmodule'}->{value} ";
    }
} else {
    $arglinex .= "$ENV{ALIEN_ROOT}/api/scripts/gapiserver/AlienAS.pl ";
}

# defined $config->{components}->{component}->{config}->{param}->{'sessionlifetime'}->{value} and $argline .= "-l $config->{components}->{component}->{config}->{param}->{'sessionlifetime'}->{value} ";

$argline .= "-c $ENV{HOME}/etc/xAlien-auto.cf -n api -l $ENV{HOME}/log/xrdlog" ;
defined $config->{components}->{component}->{config}->{param}->{'authorization_file'}->{value} and 
    $config->{components}->{component}->{config}->{param}->{'authorization_file'}->{value} =~ s/\"//g and
    $ENV{GAPI_AUTHORIZATION_FILE} = "$config->{components}->{component}->{config}->{param}->{'authorization_file'}->{value}";

$arglinex .= "$ENV{HOME}/active $nworker ";
print "Executing: $argline\n";
print "$argline\n";
print "$arglinex\n";

print "Starting xapiserver:          ... \n";

# ... start monitoring
system("$ENV{ALIEN_ROOT}/bin/alien-perl $ENV{ALIEN_ROOT}/api/scripts/gapiserver/gapiservermonitor.pl $host $port >&/dev/null &");

# ... start worker (xAlienWorker)
system("env LD_LIBRARY_PATH=$ENV{ALIEN_ROOT}/api/lib:$ENV{ALIEN_ROOT}/lib:$ENV{ALIEN_ROOT}/globus/lib:$ENV{ALIEN_ROOT}/lib/mysql  $arglinex >& $ENV{HOME}/log/xAlienWorker.log &");
# ... start xrootd (xrootd)
system("env LD_LIBRARY_PATH=$ENV{ALIEN_ROOT}/api/lib $argline >& /dev/null &");





