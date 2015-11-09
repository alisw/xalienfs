###########################################################################################
# gapi PERL module                                                                      #
# Author: A.J. Peters (libgapiUI.so by A.J.Peters & Derek Feichtinger) @ CERN/ARDA              #
###########################################################################################

###########################################################################################
# usage : my $gapi = new gapi({ host=>"x.y",port=>"5000",user=>"dummy",password=>""}};
#         my $result = $gapi->execute("ls -la");
# returns:
#         $result->{result}   => array of hash entries
#         $result->{stdout}   => stdout of this command
#         $result->{stderr}   => stderr of this command
#         
###########################################################################################

package gapi;
use strict;
use gapiUI;
use Getopt::Std;
use File::Basename;

###########################################################################################
my $self;
###########################################################################################

###########################################################################################
# the constructor
sub new {
    my $proto = shift;
    my $this  = ( shift or {} );
    my $class = ref($proto) || $proto;
    $self = (shift or {} );
    bless( $self, $class );

    $self->initialize($this,@_) or return;
    return $self;
}

###########################################################################################
# creation of a gapi UI and connetion to the gapiserver
sub initialize {
    my $self = shift;
    my $options =(shift or {});

    # the token will be stored only in memory and will not be available to other processes than this!
    $self->{gc} = gapiUI::GapiUI::MakeGapiUI(0);
    # deactivate interactive password prompt
    if (defined $options->{noprompt} && ($options->{noprompt} ne "0") && ($options->{noprompt} ne "")) {
	$ENV{GCLIENT_NOPROMPT} = "1";
    }
    if (defined $options->{nogsi} && ($options->{nogsi} ne "0") && ($options->{nogsi} ne "")) {
	$ENV{GCLIENT_NOGSI} = "1";
    }

    if (defined $options->{passwd} && $options->{passwd} ne "") {
	return $self->{gc}->Connect($options->{host},int($options->{port}),$options->{user},$options->{passwd});
    } else {
	return $self->{gc}->Connect($options->{host},int($options->{port}),$options->{user});
    }
}


###########################################################################################
# change debug mode
# debug("<modules>") to switch debug to the given modules
# <modules> is a comma seperated list of debug module names!

sub debug {
    my $self = shift;
    my $mode=(shift or "");
    if (!$self->{gc}) {
	return;
    }
    print "mode = $mode\n";
    $self->{gc}->setOption("remotedebug","$mode");
    return 1;
}

###########################################################################################
# change encryption mode
# encryption(1) to switch on encryption
# encryption(0) to switch off encryption

sub encryption {
    my $self = shift;
    my $mode=(shift or 0);
    if (!$self->{gc}) {
	return;
    }
    
    if ($mode) {
	$self->{gc}->setOption("encrypt","1");
    } else {
	$self->{gc}->setOption("noencrypt","1");
    }
    return 1;
}

###########################################################################################
# execution of arbitrary commands on the apiserver side
# - returns a hash:
# ->{stdout} : stdout of the command
# ->{stderr} : stderr of the command
# ->{result} : array of hash with the returned result values

sub execute {
    my $self = shift;
    my $execute = "";
    foreach (@_) {
	$execute .= "$_ ";
    }

    if ($execute eq "") {
	return;
    }

    my $command = $self->{gc}->Command($execute);

    my $resulthash;

    if ($command) {
	for (my $stream = 0; $stream <3; $stream++) {
	    my @result=();
	    my $nstream = $self->{gc}->GetStreamColumns($stream);
	    for (my $i=0; $i < $nstream; $i++) {
		my $ncol = $self->{gc}->GetStreamRows($stream,$i);
		my $lhash={};
		for (my $j=0; $j < $ncol; $j++) {
		    my $key   = $self->{gc}->GetStreamFieldKey($stream,$i,$j);
		    my $value = $self->{gc}->GetStreamFieldValue($stream,$i,$j);
		    if (defined $key) {
			$lhash->{$key} = $value;
		    }
		    if ($stream eq "0") {
			$resulthash->{stdout} .= $value;
		    }
		    if ($stream eq "1") {
			$resulthash->{stderr} .= $value;
		    }
		}
		if ($stream eq "2") {
		    $resulthash->{result} = \@result;
		    push @result,$lhash;
		}
		
	    }
	}
    }
    
####################################################    
# for debug purposes you can uncomment this
#    my $dumper = new Data::Dumper([$resulthash]);
#    print $dumper->Dump();
####################################################
    return $resulthash;
}

###########################################################################################
# return a boolean value from a result structure or undef, if it does not exist
sub bool {
    my $self=shift;
    my $result = (shift or return undef);
    if (!defined $result->{result}) {
	return undef;
    } else {
	if (defined ${$result->{result}}[0]->{__result__}) {
	    return ${$result->{result}}[0]->{__result__};
	} else {
	    return undef;
	}
    }
}

###########################################################################################
# return true, if the result is true, otherwise 0
sub success {
    my $self=shift;
    my $result = (shift or return 0);
    if (!defined $result->{result}) {
	return 0;
    } else {
	if (defined ${$result->{result}}[0]->{__result__}) {
	    return ${$result->{result}}[0]->{__result__};
	} else {
	    return 0;
	}
    }
}

###########################################################################################
# get a file from alien

sub get {
    my $self = shift;
    my $options = shift;
    my $src = shift;
    my $dst = shift;
    my $se  = (shift or "");
    
    if ($options !~ /s/) {
	printf("[gapi::get]: alien:$src => file:$dst [se=$se]\n");
    }
    my $whereis = $self->execute("whereis", "$src","-silent");
    if ( (! defined $whereis) || (! defined $whereis->{result}) || ( scalar @{$whereis->{result}} <= 0) ) {
	print STDERR "error: [gapi::get] cannot find out where the file $src is! Does it exist?\n";
	return 0;
    }

    my @selist;
    $#selist = -1;

    if ($se ne "") {
	push @selist, "$se";
    }

    foreach (@{$whereis->{result}}) {
	# check if it is not already there 
	my $found=0;
	my $exult;
	foreach $exult (@selist) {
	    if ( "$_->{se}" eq "$exult") {
		$found=1;
	    }
	}
	if (!$found) {
	    push @selist, $_->{se};
	}
    }

    system("touch $dst");
    if (! -w $dst) {
	print STDERR "error: [gapi::get] cannot access local file $dst\n";
	return 0;
    }

    my $error=0;
    my ($url,$md5,$guid,$envelope);
    my ($bfile,$zfile);

    foreach (@selist) {
	my $access = $self->execute("access","read","$src","$_");
	
	if ( (!defined $access->{result}) || (!defined ${$access->{result}}[0]->{"lfn"})) {
	    print STDERR "error: [gapi::get] cannot access alien file $src\n";
	    if (defined $access->{stdout} && defined $access->{stderr}) {
		print STDERR "stdout:\n$access->{stdout}\n";
		print STDERR "stderr:\n$access->{stderr}\n";
	    }
	    $error++;
	    next;
	} 
	
	# prepare the copy command
	$url  = ${$access->{result}}[0]->{"url"};
	$md5  = ${$access->{result}}[0]->{"md5"};
	$guid = ${$access->{result}}[0]->{"guid"};
	$envelope = ${$access->{result}}[0]->{"envelope"};
	
	($bfile,$zfile) = split("#",$url,2);

	my $command = "$ENV{ALIEN_ROOT}/bin/xrdcp $bfile $dst -OS\\\&authz=\"$envelope\"";
	if (run_command($command)) {
            printf STDERR "error: [gapi::get] xrdcp command failed\n";
	    $error++;
	    next;
        } else {
	    if ( -e $dst ) {
		$error=0;
		last;
	    }
	}
    }
    
    if ($error) {
	printf STDERR "error: [gapi::get] cannot get $src from any location\n";
	return 0;
    }

    # if this was a zip file
    if (defined $zfile && ($zfile ne "")) {

	if (!extract_zipfile($dst, $zfile, $dst)) {
	    unlink $dst;
	    return 0;
	}
    }

    # check the md5 of that file
    my $md5local=`md5sum $dst 2>/dev/null | awk '{print \$1}'`;
    chomp $md5local;
    if ($md5local eq "") {
	print STDERR "warning: [gapi::get] cannot calculate the md5sum - maybe md5sum is missing !\n";
    } else {
	if ("$md5local" ne "$md5") {
	    print STDERR "error: [gapi:;get] md5sum between AliEn and local file differ: local:$md5local alien:$md5\n";
	    return 0;
	}
    }

    return 1;
}

###########################################################################################
# get a file from alien

sub put {
    my $self = shift;
    my $options = shift;
    my $src = shift;
    my $dst = shift;
    my $se  = (shift or "");
    
    if ($options !~ /s/) {
	printf("[gapi::put]: file:$src => alien:$dst [se=$se]\n");
    }

    if (! -r $src) {
	print STDERR "error: [gapi::put] cannot access local file $src\n";
	return 0;
    }

    # check the md5 of that file
    my $md5local=`md5sum $src 2>/dev/null | awk '{print \$1}'`;
    chomp $md5local;
    if ($md5local eq "") {
	print STDERR "warning: [gapi::put] cannot calculate the md5sum - maybe md5sum is missing !\n";
    } 

    my $error=0;
    my ($url,$md5,$guid,$envelope);

    my $access = $self->execute("access","write-once","$dst","$se");
	
    if ( (!defined $access->{result}) || (! defined ${$access->{result}}[0]) || (!defined ${$access->{result}}[0]->{"lfn"})) {
	print STDERR "error: [gapi::put] cannot access alien file $dst\n";
	if (defined $access->{stdout} && defined $access->{stderr}) {
	    print STDERR "stdout:\n$access->{stdout}\n";
	    print STDERR "stderr:\n$access->{stderr}\n";
	}
	return 0;
    } 
    
    # check the size of that file
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
            $atime,$mtime,$ctime,$blksize,$blocks)
            = stat($src);

    # prepare the copy command
    $url  = ${$access->{result}}[0]->{"url"};
    $md5  = ${$access->{result}}[0]->{"md5"};
    $guid = ${$access->{result}}[0]->{"guid"};
    $envelope = ${$access->{result}}[0]->{"envelope"};
    
    my $command = "$ENV{ALIEN_ROOT}/bin/xrdcp $src $url -OD\\\&authz=\"$envelope\"";
    if (run_command($command)) {
	printf STDERR "error: [gapi::put] xrdcp command failed\n";
	return 0;
    }
    
    # execute the commit
    print "Envelope = $envelope\n";
    $ENV{GCLIENT_EXTRA_ARG} = "$envelope";
    my $commit = $self->execute("commit","$size","0","0","0","0","0","0","$md5local");
    delete $ENV{GCLIENT_EXTRA_ARG};

    print "$commit->{stdout}\n$commit->{stderr}\n";

    if ( (! defined $commit->{stdout}) || ( uc(($commit->{stdout}) =~ /ERROR/) ) ) {
	printf STDERR "error: [gapi::put] could not register the copied file in alien\n";
	if (defined $commit->{stdout} && defined $commit->{stderr}) {
	    print STDERR "stdout:\n$commit->{stdout}\n";
	    print STDERR "stderr:\n$commit->{stderr}\n";
	}
	return 0;
    }

    return 1;
}

sub run_command() {

    my $command = shift;
    my $result = 1;
    $result = system("$command");
    if ($result == 32512) {
	# the thread cancel error is not problematic, we can ignore it
	print STDERR "warning: libgcc_s.so.1 must be installed for pthread_cancel to work\n";
	$result = 0;
    }
    return $result;
}


sub extract_zipfile() {
    my $srcname = shift;
    my $subfile = shift;
    my $zipname = shift;
    if ((defined $subfile) && ($subfile ne "")) {
	my $tmpdir = "/tmp/.unzip_".rand().time()."__";
	mkdir $tmpdir;
	# unzip the file
	system("unzip $zipname $subfile -d $tmpdir 2>&1 > /dev/null");
	if ( -e "$tmpdir/$subfile") {
	    system("mv $tmpdir/$subfile $zipname; rmdir $tmpdir");
	    return 1;
	} else {
	    print "error: [gapi:get] cannot extract $subfile from $zipname\n";
	    
	}
    }
    return 0;
}




###########################################################################################
return 1;
###########################################################################################
