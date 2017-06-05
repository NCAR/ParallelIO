#!/usr/bin/env perl
use strict;
use warnings;

use Getopt::Long;

my $rundir="";
my $exe="";
my $nargs = 0;
my $verbose = 0;
my $is_dry_run = 0;

# Reg expression that match the pio decomposition file names
my $PIO_DECOMP_FNAMES = "^piodecomp";
my $BEGIN_STACK_TRACE = "Obtained";

my $TRAILER_SIZE_INVALID = -1;

# Compares two decomp files and returns true/1 if they are equal
# else returns false/0
sub cmp_decomp_files
{
    my ($f1name, $f2name, $trsize_ref) = @_;
    ${$trsize_ref} = 0;
    open(F1,$f1name);
    my @file1 = <F1>;
    open(F2,$f2name);
    my @file2 = <F2>;
    my $rmfile = 1;
    foreach my $f1line (@file1){
        my $f2line = shift (@file2);
        # Ignore stack traces when comparing files
        # The stack traces start with a line containing
        # "Obtained" 
        # Also, stack trace is the last line being
        # compared
        if(($f1line =~ /${BEGIN_STACK_TRACE}/)
              && ($f2line =~ /${BEGIN_STACK_TRACE}/)){
            # Calculate trailer length/size in bytes
            use bytes;
            ${$trsize_ref} += length($f1line);
            while(defined(my $trline = shift(@file2))){
                ${$trsize_ref} += length($trline);
            }
            if($verbose){
                print "Files $f1name and $f2name are the same (ignoring stack traces), trailer sz = ${$trsize_ref}\n";
            }
            last;
        }
        next if($f1line eq $f2line);
        # Files are different, don't remove    
        $rmfile = 0;
        last;
    }
    close(F1);
    close(F2);
    return ($rmfile == 1);
}

# Get the size of the trailer in the decomposition file
# Trailer => Content of the file after the stack trace
sub get_decomp_trailer_sz
{
    my ($fname) = @_;
    my $ftrsize = 0;
    my $has_trailer = 0;
    open(F1,$fname);
    # Read the file from the end
    my @file1 = reverse <F1>;
    foreach my $line (@file1){
        # Trailer starts with the stack trace
        # The stack traces start with a line containing
        # "Obtained" 
        # Calculate trailer size/length in bytes
        use bytes;
        $ftrsize += length($line);
        if($line =~ /${BEGIN_STACK_TRACE}/){
            $has_trailer = 1;
            last;
        }
        next;
    }
    close(F1);
    if(!$has_trailer){
        # If the trailer is not present we end up with
        # the size of the decomp file as the trailer size,
        # so reset it
        $ftrsize = 0;
    }
    return $ftrsize;
}

# Remove duplicate decomposition files in "dirname"
sub rem_dup_decomp_files
{
    my($dirname) = @_;
    # Find files in current directory that are
    # named *piodecomp* - these are the pio 
    # decomposition files
    opendir(F,$dirname);
    #my @decompfiles = grep(/^piodecomp/,readdir(F));
    my @decompfile_info_tmp = map{ {FNAME=>"$dirname/$_", SIZE=>-s "$dirname/$_", TRAILER_SIZE=>$TRAILER_SIZE_INVALID, IS_DUP=>0} } grep(/${PIO_DECOMP_FNAMES}/,readdir(F));
    closedir(F);
    my @decompfile_info = sort { $a->{SIZE} <=> $b->{SIZE} } @decompfile_info_tmp;
    my $ndecompfile_info = @decompfile_info;

    #for(my $i=0; $i<$ndecompfile_info; $i++){
    #  print "File : $decompfile_info[$i]->{FNAME} , size = $decompfile_info[$i]->{SIZE}\n";
    #}
    
    my $rmfile=0;
    # Compare the decomposition files to find
    # duplicates - and delete the dups
    for(my $i=0; $i<$ndecompfile_info; $i++){
        my $f1name  = $decompfile_info[$i]->{FNAME};
        my $f1size  = $decompfile_info[$i]->{SIZE};
        next if($decompfile_info[$i]->{IS_DUP});
        for(my $j=$i+1;$j<$ndecompfile_info;$j++){
            my $f2name = $decompfile_info[$j]->{FNAME};
            my $f2size = $decompfile_info[$j]->{SIZE};
            next if($decompfile_info[$j]->{IS_DUP});
            last if($f1size != $f2size);
            if($verbose){
                print "Comparing $f1name, size=$f1size, $f2name, size=$f2size\n";
            }
            if($f1size == $f2size){
                my $trsize = 0;
                $rmfile = &cmp_decomp_files($f1name, $f2name, \$trsize);
                if($rmfile){
                    $decompfile_info[$i]->{TRAILER_SIZE} = $trsize;
                    $decompfile_info[$j]->{TRAILER_SIZE} = $trsize;
                    $decompfile_info[$j]->{IS_DUP} = 1;
                    if($is_dry_run){
                        print "\"$decompfile_info[$j]->{FNAME}\" IS DUP OF \"$decompfile_info[$i]->{FNAME}\", trailer sz = $trsize\n";
                    }
                }
            }
        }
        if($decompfile_info[$i]->{TRAILER_SIZE} == $TRAILER_SIZE_INVALID){
            $decompfile_info[$i]->{TRAILER_SIZE} = &get_decomp_trailer_sz($f1name);
        }
    }

    # Compare files ignoring the trailer
    # - Compare files that have the same size after ignoring the trailer
    for(my $i=0; $i<$ndecompfile_info; $i++){
        my $f1name  = $decompfile_info[$i]->{FNAME};
        my $f1size  = $decompfile_info[$i]->{SIZE};
        my $f1trsize  = $decompfile_info[$i]->{TRAILER_SIZE};
        next if($decompfile_info[$i]->{IS_DUP});
        for(my $j=$i+1;$j<$ndecompfile_info;$j++){
            my $f2name = $decompfile_info[$j]->{FNAME};
            my $f2size = $decompfile_info[$j]->{SIZE};
            my $f2trsize = $decompfile_info[$j]->{TRAILER_SIZE};
            next if($decompfile_info[$j]->{IS_DUP});
            if($verbose){
                print "Comparing files $f1name, adj sz = ($f1size - $f1trsize), $f2name, adj sz = ($f2size - $f2trsize)\n";
            }
            next if(($f1size - $f1trsize) != ($f2size - $f2trsize));
            if($verbose){
                print "Comparing $f1name, size=$f1size, trsize=$f1trsize, $f2name, size=$f2size, trsize=$f2trsize\n";
            }
            my $trsize = 0;
            $rmfile = &cmp_decomp_files($f1name, $f2name, \$trsize);
            if($rmfile){
                $decompfile_info[$j]->{IS_DUP} = 1;
                if($is_dry_run){
                    print "\"$decompfile_info[$j]->{FNAME}\" IS DUP OF \"$decompfile_info[$i]->{FNAME}\", trailer sz = $trsize\n";
                }
            }
        }
    }
    for(my $i=0; $i<$ndecompfile_info; $i++){
        if($is_dry_run == 0){
            if($decompfile_info[$i]->{IS_DUP}){
                unlink($decompfile_info[$i]->{FNAME});
            }
        }
    }
    if($is_dry_run){
        print "UNIQUE files are : ";
        for(my $i=0; $i<$ndecompfile_info; $i++){
            if($decompfile_info[$i]->{IS_DUP} == 0){
                print "\"$decompfile_info[$i]->{FNAME}\" (trailer sz = $decompfile_info[$i]->{TRAILER_SIZE}), ";
            }
        }
        print "\n";
    }
}

# Decode the stack traces in the pio decomposition files
sub decode_stack_traces
{
    # dirname => Directory that contains decomp files
    # exe => executable (including path) that generated
    #         the decomposition files
    my($dirname, $exe) = @_;
    # Decode/Translate the stack trace
    opendir(F,$dirname);
    my @decompfiles = grep(/${PIO_DECOMP_FNAMES}/,readdir(F));
    closedir(F);
    my $ndecompfiles = @decompfiles;
    for(my $i=0; $i< $ndecompfiles; $i++){
        my $fname = "$dirname/$decompfiles[$i]";
        open(F1,$fname);
        my @file1 = <F1>;
        close(F1);
        if(!$is_dry_run){
            open(F1,">$fname");
        }
        else{
            print "Decoded stack frame ($fname) :\n";
        }
        foreach(@file1){
            # Find stack addresses in the file and use
            # addrline to translate/decode the filenames and
            # line numbers from it
            my $outline = $_;
            if(/\[(.*)\]/){
                my $outline = `addr2line -e $exe $1`;
                if($verbose || $is_dry_run){
                    print  "$outline\n";
                }
            }
            if(!$is_dry_run){
                print F1 "$outline\n";
            }
        }
        if(!$is_dry_run){
            close(F1);
        }
    }
}

sub print_usage_and_exit()
{
    print "\nUsage :\n./prune_decomps.pl --decomp-prune-dir=<PRUNE_DECOMP_DIR> \n";
    print "\tOR\n";
    print "./prune_decomps.pl <PRUNE_DECOMP_DIR> \n";
    print "The above commands can be used to remove duplicate decomposition\n";
    print "files in <PRUNE_DECOMP_DIR> \n";
    print "Available options : \n";
    print "\t--decomp-prune-dir : Directory that contains the decomp files to be pruned\n";
    print "\t--exe      : Executable that generated the decompositions \n";
    print "\t--dry-run  : Run the tool and only print (without pruning) dup decomp files\n";
    print "\t--verbose  : Verbose debug output\n";
    exit;
}

# Main program

# Read input args
GetOptions(
    "decomp-prune-dir=s"    => \$rundir,
    "exe=s"             => \$exe,
    "dry-run"               => \$is_dry_run,
    "verbose"               => \$verbose
);

$nargs = @ARGV;

if($rundir eq ""){
    $rundir = shift;
    if($rundir eq ""){
        &print_usage_and_exit();
    }
}
if($verbose){ print "Removing duplicate decomposition files from : \"", $rundir, "\"\n"; }
&rem_dup_decomp_files($rundir);

if($exe ne ""){
    if($verbose){ print "Decoding stack traces for decomposition files from : \"", $rundir, "\"\n"; }
    &decode_stack_traces($rundir, $exe);
}

    
