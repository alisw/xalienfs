########################################################################################
# AlienAS.pl 
# - apiservice perl command executor and filter
# - configuration file is $ENV{HOME}/etc/gapiservice.cfg.xml
# --> for debug, set debug parameter in XML file to value != 0
########################################################################################
use File::Basename;
use Data::Dumper;
use XML::Simple;
use AliEn::UI::Catalogue::LCM::Computer;
#use AliEn::UI::Catalogue::LCM;
use Time::HiRes qw( gettimeofday tv_interval );
my $xsimple = XML::Simple->new();

$config = $xsimple->XMLin("$ENV{HOME}/etc/gapiservice.cfg.xml",
#				   KeyAttr => {event => 'name', file => 'name', mirror => 'name'},
                                   ForceArray => [ 'arrayvalue'  ],
				   ContentKey => '-content');
$config or print STDERR "AlienAS config: cannot find configuration file $ENV{HOME}/etc/gapiservice.cfg.xml !\n" and exit;

# stream special characters according to CODEC.h
my $streamend       = chr 0;
my $fieldseparator  = chr 1;
my $fielddescriptor = chr 2;
my $columnseparator = chr 3;
my $stdoutindicator = chr 4;
my $stderrindicator = chr 5;
my $outputindicator = chr 6;
my $outputterminator = chr 7;

use Getopt::Long ();
my $now = time;
print "--------- PERL Starting up $now ----------\n";
open ENVINPUT , "alien --printenv|";
while (<ENVINPUT>) {
    my ($name,$value) = split('=',$_,2);
    chomp $name;
#    $ENV{"$name"} = $value;
#    print "Setting Environment $name=$value\n";
}
close ENVINPUT;

$ENV{ALIEN_COLORTERMINAL}="1";
#############################################################################
# reading the configuration options
#############################################################################

if ( ( ! defined $config->{components}->{component}->{name}) ||
     $config->{components}->{component}->{name} ne "gapiservice" ) {
    print STDERR "Configuration file does not contain the component name 'gapiservice' - fix it!\n";
    exit;
}

if (defined $config->{components}->{component}->{config}->{param}->{'user'}->{value}) {
    $config->{components}->{component}->{config}->{param}->{'user'}->{value} =~ s/\"//g;
    $ENV{'ALIEN_USER'} = $config->{components}->{component}->{config}->{param}->{'user'}->{value};
}

if (defined $config->{components}->{component}->{config}->{param}->{'role'}->{value}) {
    $config->{components}->{component}->{config}->{param}->{'role'}->{value} =~ s/\"//g;
    $ENV{'ALIEN_ROLE'} = $config->{components}->{component}->{config}->{param}->{'role'}->{value};
}

my $port=1094;
if (defined $config->{components}->{component}->{config}->{param}->{'port'}->{value}) {
    $port = $config->{components}->{component}->{config}->{param}->{'port'}->{value};
}

my $debug = 0;
my $commandlogging = 0;
my $commandlogfile = "";

defined $config->{components}->{component}->{config}->{param}->{'debug'}->{value} and
    $debug = $config->{components}->{component}->{config}->{param}->{'debug'}->{value} and print "Setting configuration: debug = \"$debug\"\n";

defined $config->{components}->{component}->{config}->{param}->{'commandlogging'}->{value} and
    $commandlogging = $config->{components}->{component}->{config}->{param}->{'commandlogging'}->{value} and print "Setting configuration: commmandlog = \"$commandlogging\"\n";

defined $config->{components}->{component}->{config}->{param}->{'commandlogfile'}->{value} and
    $commandlogfile = $config->{components}->{component}->{config}->{param}->{'commandlogfile'}->{value} and print "Setting configuration: commmandlogfile = \"$commandlogfile\"\n";
    

if ($commandlogfile ne "") {
    if ( -x "$ENV{ALIEN_ROOT}/sbin/cronolog" ) {
	my $dlogfile = dirname $commandlogfile;
	my $blogfile = basename $commandlogfile;
	open CMDLOG , " | $ENV{ALIEN_ROOT}/sbin/cronolog $dlogfile/\%Y/\%m/\%d_\%p/$blogfile"; 
    } else { 
 	open CMDLOG , ">>$commandlogfile";
    }
}

$debug and system("printenv| grep ALIEN");
if (defined $config->{components}->{component}->{config}->{param}->{environment}->{arrayvalue} ) {
    foreach (@{$config->{components}->{component}->{config}->{param}->{environment}->{arrayvalue}}) {
	$_ =~ s/\"//g;
	my ($key,$val) = split ("=",$_);
	if (defined $key){
	    print "Setting Environement:     $key = $val\n";
	    $ENV{$key} = $val;
	}
    }
}

my $catalogue;

    $catalogue= new AliEn::UI::Catalogue::LCM::Computer({user=>"$ENV{'ALIEN_USER'}",role=>"$ENV{'ALIEN_ROLE'}"});



if (!$catalogue) {
    printf STDERR "! Fatal Error in PERL: Cannot connect to the Catalogue";
    exit(-1);
}

my @filteredcommands=("quit", "exit","user");

if (defined $config->{components}->{component}->{config}->{param}->{filteredcommands}->{arrayvalue} ) {
    foreach (@{$config->{components}->{component}->{config}->{param}->{filteredcommands}->{arrayvalue}}) {
	push @filteredcommands, $_;
    }
}

foreach (@filteredcommands) {
    if ($debug){ logcmd("Filtering command: ","$_");}
}


sub logcmd {
    my $now = `date`;
    chomp $now;
    if ($commandlogfile ne "") {
	my $string = sprintf "[%05d] $now @_\n",$$;
	syswrite (CMDLOG, $string , (length $string));
    } else {
	printf "[%05d] $now @_\n",$$;
    }
}


sub mytmpname(){
    return ("/tmp/". time() . $$ . rand());
}

sub Test {
    my $joinedarg = shift;
    my $seperator = chr 1;
    my @args = split (/$seperator/,$joinedarg);
    my $opt;
    print "------------------------------------\n";
    print "Hello, C-world, this is Perl!\n";
    print "------------------------------------\n";
    print "Args: $joinedarg\n";
    foreach (@args) {
	print "Arg: $_\n";
    }
    print "------------------------------------\n";
    return "OK";
}

sub Silent {
    my $joinedarg = shift;
    my $seperator = chr 1;
    my @args = split (/$seperator/,$joinedarg);
    foreach (@args) {
	my $tmp = $_;
    }
    return "OK";
}

sub Callfilter {
    my $clientstate = shift;
    my $cmd = shift;
    my $forbidden="You cannot execute $cmd!";

    foreach ( @filteredcommands) {
	if ($cmd eq "$_") {
	    print "Command $forbidden is forbidden\n";
	    return \$forbidden;
	}
    }
    
    if ( $cmd =~ /\w\.\w/ ) {
	print "Command $forbidden is forbidden\n";
	return \$forbidden;
    }
    
    my $result;
    if ($clientstate->{pwd} eq "") {
	$clientstate->{pwd} = $catalogue->{CATALOG}->f_pwd("-s") ;
    }
    if ($cmd eq "cd" ) {
	$result=$catalogue->execute($cmd,@_);
	$clientstate->{pwd} = $catalogue->{CATALOG}->f_pwd("-s") ;
    } else {
	if ($cmd eq "commit") {
	    $result=$catalogue->{CATALOG}->commit(@_);
	} else {
	    if ($cmd eq "localsubmit") {
		my @arrayresult=$catalogue->{QUEUE}->submitCommand("-z","==<",@_);
		$result = \@arrayresult;
	    } else {
		if ($cmd eq "motd") {
		    if ( -e "$ENV{HOME}/.alien/apiservice.motd") {
			open READFD,"$ENV{HOME}/.alien/apiservice.motd";
			while (<READFD>) {
			    print $_;
			}
			close READFD;
		    }
		} else {
		    if ($cmd eq "who") {
			$opt = ( shift or "");
			if ($opt =~ /l/) {
			    $result = system("cat /tmp/shmstat_$port.txt | awk '{printf(\"%16s %06d %s\\n\", \$2, \$4, \$9);}' | sort -g ");
			} else {
			    $result = system("cat /tmp/shmstat_$port.txt | awk '{print \$2}' | sort | uniq");
			}
		    } else {
			if ($cmd eq "find" ) { 
			    $result=$catalogue->execute($cmd,"-z",@_);
			} else {
				if ( ($cmd eq "ls" ) || ($cmd eq "whereis") || ($cmd eq "access") || ($cmd eq "ps2") || ($cmd eq "getdataset") || ($cmd eq "submit") || ($cmd eq "packman")) { 
				    $result=$catalogue->execute($cmd,"-z",@_);
				} else {
				    if ($cmd eq "AliEn::Catalogue") {
					$result = undef;
					my $internalcommand = shift @_;
					if ( $internalcommand ne "f_disconnect") {
					    eval { $result=$catalogue->{CATALOG}->$internalcommand(@_); };
					    if ($@) {
						$result = undef;
					    }
					} else {
					    $result = 1;
					}
				    } else {
					if ($cmd eq "mkdir") {
					    ($result) = $catalogue->execute($cmd,@_);
					    if (! defined $result) {$result=0;}
					} else {
					    $result=$catalogue->execute($cmd,@_);
					}
				    }
				}
			    }
		    }
		}
	    }
	}
    }
    return $result;
}

sub LocalCall {
    my $joinedarg = shift;
    my $fieldseparator  = chr 1;
    my ($empty,$cmd,@args) = split (/$columnseparator/,$joinedarg);
    $debug and printf "joined $joinedargs Command is $cmd args are @args\n";
    my $result;

    my $t0 = [gettimeofday];

    $commandlogging and logcmd("LocalCall", "$cmd","-z",@args);

    $result=$catalogue->execute($cmd,"-z",@args);

    my $t1 = sprintf "%.02f", tv_interval( $t0, [gettimeofday], );
    
    if ($cmd eq "verifySubjectRole") {
	if ((defined @$result) && (defined @$result[0])) {
	    $debug and printf "Returning @#result[0]->{role}\n";
	    $commandlogging and logcmd("CertLogin", "elapsed=$t1 User=@$result[0]->{role} args=@args");
	    return @$result[0]->{role};
	} else {
	    $debug and printf "Returning nothing\n";
	    $commandlogging and logcmd(" CertFail", "elapsed=$t1 $args=@args");
	    return "";
	}
    }
    if ((defined @$result) && (defined @$result[0])) {
	$commandlogging and logcmd(" TokLogin", "elapsed=$t1 User=@$result[0] args=@args");
	return @$result[0];
    } else {
	$commandlogging and logcmd("  TokFail", "elapsed=$t1 args=@args");
	return "";
    }
}


sub Call {
    my $joinedarg = shift;
    my $length = length $joinedarg;

#    my ($empty,$user,$clientstatestr,@args) = split (/$columnseparator/,$joinedarg);

    my ($cmdstr,$clientstatestr) = split(/$outputterminator/,$joinedarg);
    my ($empty,$user,@args) = split(/$columnseparator/,$cmdstr);

    my $cmd = "Callfilter";
    $debug and print "------------------------------------\n";
    $debug and print "Args: $joinedarg\n";
    $debug and print "Cmd: $cmd\n";
    $debug and print "Length: $length\n";
    $debug and print "Clientstate: >$clientstatestr<\n";

    my $t0 = [gettimeofday];
    my $cnt=0;

    foreach (@args) {
	$debug and print "Arg $cnt\t:\t $_\n";
	$cnt++;
    }

    $commandlogging and logcmd("     Call", "User=$user", @args);


    my ($ret,$clientstate) = codecToHash($clientstatestr);
    if($ret==-1) {
	die "(Call) unable to decode client state: $$clientstate{errordetail}";
    } else {
	foreach my $elem (keys %$clientstate) {
	   $debug and  print "  client state: $elem => >$clientstate->{$elem}<\n";
	}
    }
    
    if(!exists($clientstate->{"pwd"}) or $clientstate->{"pwd"} eq "") {
	$clientstate->{pwd}="";
  }

    $debug and print "User: $user\n";
    $debug and print "------------------------------------\n";

    # executing the su command
    $catalogue->{CATALOG}->f_user("-","$user") or return  errorstream(GLITE_ERROR_ILLEGALUSER,"FATAL: Cannot take the identity of $user!",$clientstate);

    # see if we have to enable the debug mode
    if (exists $clientstate->{"remoteDebug"} and $clientstate->{"remoteDebug"} ne "") {
	my $modules = $clientstate->{"remoteDebug"};
	$modules =~ s/\"//g;
	my @amodules = split (",", $modules);
	$debug and print "Debug Modules: @amodules\n";
	$catalogue->setDebug(@amodules);
    }
    open SAVE_STDOUT,">&STDOUT";
    open SAVE_STDERR,">&STDERR";
    my $tmpstdout = mytmpname();
    my $tmpstderr = mytmpname();
    $debug and print "$tmpstdout\n$tmpstderr\n";
    open STDOUT,"> $tmpstdout";
    open STDERR,"> $tmpstderr";
    open STDOUTTMP,"$tmpstdout";
    open STDERRTMP,"$tmpstderr";


    # allow to execute 'cd /...' without changing the environment working directory
    if (!( ($args[0] eq "cd") && ( ($args[1] =~ /^\// ) || ($#args == 0)))) {
	if(! $catalogue->{CATALOG}->f_cd($clientstate->{pwd}) ) {
	  $clientstate->{pwd} = $catalogue->{CATALOG}->f_pwd("-s");
	  return errorstream(GLITE_ERROR_ILLEGALDIR,"FATAL: Cannot change to the working directory $clientstate->{pwd}, changing to home directory\n",$clientstate);
	}
#	printf "I am changing to $clientstate->{pwd}";
    } else {
#	printf "I do not change the working directory\n";
    }
	   

    my $cmdref = \&{$cmd};
    my $result = &$cmdref($clientstate,@args);

    close STDOUT;
    close STDERR;
    open STDOUT,">&SAVE_STDOUT";
    open STDERR,">&SAVE_STDERR";
    
    my $returnstdoutstream="";
    my $returnstderrstream="";
    my $returnargstream;

    my @stdoutoutput = <STDOUTTMP>;
    my @stderroutput = <STDERRTMP>;

    close STDOUTTMP;
    close STDERRTMP;

    unlink $tmpstdout;
    unlink $tmpstderr;

    $returnstdoutstream = $columnseparator.$fieldseparator;
    $returnstderrstream = $columnseparator.$fieldseparator;
    $returnstdoutstream .= join "$columnseparator.$fieldseparator", @stdoutoutput;
    $returnstderrstream .= join "$columnseparator.$fieldseparator", @stderroutput;

    $debug and print "STDOUT1: @stdoutoutput\n";
    $debug and print "STDERR2: @stderroutput\n";

    my $calledfunction = shift @args;
    if ($calledfunction eq "AliEn::Catalogue") {
	$Data::Dumper::Indent = 0;
	$returnargstream .= $columnseparator;
	$returnargstream .= $fielddescriptor;
	$returnargstream .= "__Dumper__";
	$returnargstream .= $fieldseparator;
	$returnargstream .= Dumper($result);
    } else {
	my $type = ref($result);
	
	if ( $type eq "HASH" ) {
	    $returnargstream .= $columnseparator;
	    foreach ( keys %$result ) {
		$returnargstream .= $fielddescriptor;
		$returnargstream .= $_;
		$returnargstream .= $fieldseparator;
		$returnargstream .= $result->{$_};
		
	    }
	}
	
	if ( ($type eq "SCALAR") || ($type eq "") ) {
	    my $cscalar = \$result;
	    if ( ref($cscalar) eq "SCALAR" ) {
		$returnargstream .= $columnseparator;
		$returnargstream .= $fielddescriptor;
		$returnargstream .= "__result__";
		$returnargstream .= $fieldseparator;
		$returnargstream .= $result;
	    }
	    if ( ref($cscalar) eq "ARRAY" ) {
		foreach (@$cscalar) {
		    $returnargstream .= $columnseparator;
		    $returnargstream .= $fielddescriptor;
		    $returnargstream .= "__result__";
		    $returnargstream .= $fieldseparator;
		    $returnargstream .= $_;
		}
	    }
	}
	
	if ( $type eq "ARRAY") {
	    my $larray;
	    foreach $larray ( @$result ) {
		$returnargstream .= $columnseparator;
		if ( ref($larray) eq "HASH" ) {
		    my $lkeys;
		    foreach $lkeys ( keys %$larray ) {
			$returnargstream .= $fielddescriptor;
			$returnargstream .= $lkeys;
			$returnargstream .= $fieldseparator;
			$returnargstream .= $larray->{$lkeys};
			
		    }
		}
		if ( (ref($larray) eq "SCALAR") || (ref($larray) eq "") ){
		    $returnargstream .= $fielddescriptor;
		    $returnargstream .= "__result__";
		    $returnargstream .= $fieldseparator;
		    $returnargstream .= $larray;
		}
	    }
	}
    }

    $envhash{"pwd"} = $clientstate->{"pwd"};

    if ( -e "$ENV{HOME}/.alien/apiservice.sysmsg") {
	open READFD,"$ENV{HOME}/.alien/apiservice.sysmsg";
	my $sysmsg="";
	while (<READFD>) {
	    $sysmsg .= $_;
	}
	close READFD;
	$envhash{"sysmsg"} = $sysmsg;
    }

    my $clientenv = $columnseparator . codecEncodeHash(\%envhash);

    # switch off the debug 
    if (exists $clientstate->{"remoteDebug"} and $clientstate->{"remoteDebug"} ne "") {
	$catalogue->setDebug(0);
    }

    my $t1 = sprintf "%.02f", tv_interval( $t0, [gettimeofday], );
    $commandlogging and logcmd("     Call", "User=$user elapsed=$t1", @args);

    return ( $stdoutindicator . $returnstdoutstream . $stderrindicator . $returnstderrstream . $outputindicator . $returnargstream . $outputterminator . $clientenv . $streamend);
}

sub testls {
    my @resarray;
    my $cnt=0;
    my @args = @_;
    foreach (@args) {
	$cnt++;
	my $now = time;
	my $set={};
	$set->{time} = $now;
	$set->{cnt}  = $cnt;
	$set->{name} = $_;
	push @resarray, $set;
	print "=> $now $cnt $_\n";
    }
    return \@resarray;
}

############################################################
# returns a reference to a hash containing the decoded key value
# pairs from the given string
#
# @param string string containing the encoded key value pairs
# @return 2 element array containing exit state in first field
#         and the reference to the result hash in the second.
#         Exit state is -1 in case of error.

sub codecToHash {
  my $instring=shift;

  $instring =~ s/${columnseparator}(.*)/$1/;
  my %hash=();
  my @keyvalpairs=split(/${fielddescriptor}/,$instring);
  shift @keyvalpairs; # first item is empty

  if($#keyvalpairs == -1) {
    %hash=("errordetail" => "no fields found");
    return(-1,\%hash);
  }

  my $item;
  foreach $item (@keyvalpairs) {
    my ($key,$value)=split(/$fieldseparator/,$item);
    if(!defined($value)) {
      %hash=("errordetail" => "no value for key >$key<");
      return(-1,\%hash);
    }
    $hash{$key}=$value;
  }
  return(0,\%hash);
}

sub codecEncodeHash {
  my $hashref=shift;
  my $res;
  foreach ( keys %$hashref ) {
    #print "encodeHash: $_ => $hashref->{$_}";
    $res .= $fielddescriptor;
    $res .= $_;
    $res .= $fieldseparator;
    $res .= $hashref->{$_};
  }
  return $res;
}

sub codecEncodeArray {
  $arrayref=shift;
  my $elem;
  my $res="";
  foreach $elem (@$arrayref) {
    $res .= $columnseparator;
    if(ref($elem) eq "HASH") {
      codecEncodeHash($elem);
    } else { # SCALAR
      $res .= $fieldseparator;
      $res .= $elem;
    }
  }
  return $res;
}

sub codecEncodeScalar {
  $scalar=shift;
  my $res="";
  $res .= $columnseparator;
  $res .= $fieldseparator;
  $res .= $scalar;
  return $res;
}


sub errorstream {
    my $errorval     = shift;
    my $errormessage = shift;
    my $clientstate    = shift;
    $clientstate->{error}    = "$errorval";
    $clientstate->{errortxt} = "$errormessage";
    my $clientenv = $columnseparator . codecEncodeHash($clientstate);

    return ( $stdoutindicator . codecEncodeScalar("") . $stderrindicator . codecEncodeScalar("$errormessage") . $outputindicator . codecEncodeScalar("") . $outputterminator . $clientenv . $streamend );
}


