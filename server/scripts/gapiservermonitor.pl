use AliEn::Config;
use strict;

my $host = $ARGV[0];
my $port = $ARGV[1];

my $config = new AliEn::Config();
$config or return;

# setup the monalisa configuration
if($config->{MONALISA_HOST} || $config->{MONALISA_APMONCONFIG}) {
    eval "require ApMon";
    if($@){
	print STDERR "ApMon module is not installed; skipping monitoring\n";
	exit(-1);
    }
    my $apmon = ApMon->new(0);
    if($config->{MONALISA_APMONCONFIG}){
        my $cfg = eval($config->{MONALISA_APMONCONFIG});
        $apmon->setDestinations($cfg);
        if(ref($cfg) eq "HASH"){
	    my @k = keys(%$cfg);
	    $cfg = $k[0];
        }elsif(ref($cfg) eq "ARRAY"){
	    $cfg = $$cfg[0];
        }
        $ENV{APMON_CONFIG} = $cfg;
    }else{
        my $cfg = $config->{MONALISA_HOST};
        $apmon->setDestinations([$cfg]);
        $ENV{APMON_CONFIG} = $cfg;
    }
    # set the default cluster and node
    $apmon->sendParameters("$config->{SITE}_ApiService", "$host:$port");
    # afterwards I'll use just sendParams regardless of service name

    my $oldncmd=0;
    my $oldtime=0;
    my $ncmd_R=0;
    while (1) {
	my $file = "/tmp/shmstat_$port.txt";

	my $nsession       = `cat $file | grep user | wc -l | awk '{print \$1}'`;
	my $nuser          = `cat $file| grep user | awk '{print \$2}' | sort | uniq | wc -l | awk '{print \$1}'`;
	my $nuseractive    = `cat $file| grep user | awk '{if (\$4 < 300) print \$2}' | sort | uniq | wc -l | awk '{print \$1}'`;
	my $nsessionvalid  = `cat $file | grep user | grep valid | wc -l | awk '{print \$1}'`;
	my $nsessionactive = `cat $file | grep user | awk '{if (\$4 <300) print \$2,\$4;}' | wc -l | awk '{print \$1}'`;
	my $ncmd           = `head -1 $file | awk '{print \$5 ;}' `;
	my $ntotalsession  = `head -1 $file | awk '{print \$3 ;}' `; 

	chomp $nsession;
	chomp $nuser;
	chomp $nuseractive;
	chomp $nsessionvalid;
	chomp $nsessionactive;
 	chomp $ncmd;
	chomp $ntotalsession;
	my $now = time;
	my @params;
	my $deltat;
	push @params, "n_sessions";
	push @params, $nsession;
	push @params, "n_user";
	push @params, $nuser;
	push @params, "n_user_active";
	push @params, $nuseractive;
	push @params, "n_sessions_valid";
	push @params, $nsessionvalid;
	push @params, "n_sessions_active";
	push @params, $nsessionactive;
	push @params, "n_cmdcnt";
	push @params, $ncmd;
	push @params, "n_total_sessions";
	push @params, $ntotalsession;

	if ($oldncmd !=0 ) {
	    if ($oldtime != 0 ) {
		$deltat = ($now - $oldtime);
		if ($deltat != 0) {
		    $ncmd_R = ($ncmd - $oldncmd) / ($deltat) * 60;
		    if ($ncmd_R >=0) {
			push @params, "n_cmdcnt_R";
			push @params, "$ncmd_R";
		    }
		}
	    }
	}
	
	$oldtime = $now;
	$oldncmd = $ncmd;
	print "$now: $nsession $nuser $nuseractive $nsessionvalid $nsessionactive $ncmd [$ncmd_R] $ntotalsession\n";
	$apmon->sendParams(@params);	
	$apmon->sendBgMonitoring();
	sleep(120);
    }
}
