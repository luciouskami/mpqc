# Emacs should use -*- Perl -*- mode.

$srcdir = $ARGV[0];

if (substr($srcdir,0,1) eq "/") {
    $topdir = "";
}
else {
    $topdir = ".";
}

$stubhead = "# Generated by objectdir.pl -- edit at own risk.\n";

&dodir("$srcdir",".",$topdir);

exit;

sub dodir {
    local($dir,$objdir,$topdir) = @_;
    local($file);
    local(@files);

    #print "In directory $dir\n";

    opendir(DIR, $dir) || (warn "Can't open $dir: $!\n", return);
    @files = readdir(DIR);
    closedir(DIR);

    foreach $file (@files) {
        if ($file eq "." || $file eq ".." || $file eq "CVS") {
            next;
        }

        local($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
         $atime,$mtime,$ctime,$blksize,$blocks)
            = stat("$dir/$file");

        if (-d "$dir/$file") {
            mkdir ("$objdir/$file", $mode);
            local($nexttop);
            if ($topdir eq ".") {
                $nexttop = "../";
            }
            elsif ($topdir eq "" ) {
                $nexttop = "";
            }
            else {
                $nexttop = "$topdir../";
            }
            &dodir("$dir/$file", "$objdir/$file", $nexttop);
        }
        elsif ("$file" eq "Makefile") {
            #print "Found $dir/Makefile\n";
            local($nextdir);
            &domake("$topdir$dir", "$objdir/$file");
        }
    }
}

sub domake {
    local($topdir, $stubmake) = @_;

    if (-f $stubmake) {
        open(STUBMAKE,"<$stubmake");
        local($line) = scalar(<STUBMAKE>);
        close(STUBMAKE);
        if ($line eq $stubhead) {
            print "Overwriting "
        }
        else {
            print "Skipping $stubmake\n";
            return;
        }
    }
    else {
        print "Writing ";
    }
    print "$stubmake\n";

    open(STUBMAKE,">$stubmake");
    print STUBMAKE "$stubhead";
    print STUBMAKE "SRCDIR = $topdir\n";
    print STUBMAKE "VPATH = \$(SRCDIR)\n";
    print STUBMAKE "include \$(SRCDIR)/Makefile\n";
    close(STUBMAKE);
}
