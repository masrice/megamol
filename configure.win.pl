#
# configure.win.pl
# MegaMol Plugin
#
# Copyright (C) 2008-2010 by VISUS (Universitaet Stuttgart).
# Alle Rechte vorbehalten.
#
push @INC, "configperl";
require "configperl.inc";

my @pps = ();
my @fps = ();
my @cfps = ();
my @sps = ();

$a = PathParameter->new();
    $a->id("outbin");
    $a->description("Path to the global \"bin\" output directory");
    $a->placeholder("%outbin%");
    $a->autoDetect(0);
    push @pps, $a;
$a = PathParameter->new();
    $a->id("outshader");
    $a->description("Path to the global \"shaders\" output directory");
    $a->placeholder("%outshader%");
    $a->autoDetect(0);
    push @pps, $a;

$a = PathParameter->new();
    $a->id("mmcore");
    $a->description("Path to the MegaMol core directory");
    $a->placeholder("%mmcorePath%");
    $a->markerFile("api/MegaMolCore.h\$");
    $a->relativeLocation("../");
    $a->autoDetect(1);
    push @pps, $a;
$a = PathParameter->new();
    $a->id("vislib");
    $a->description("Path to the vislib directory");
    $a->placeholder("%vislib%");
    $a->markerFile("vislib.sln\$");
    $a->relativeLocation("./");
    $a->autoDetect(1);
    push @pps, $a;
$b = FlagParameter->new();
    $b->id("withNETCDF");
    $b->description("Enable the use of the NetCDF library");
    $b->placeholder("%withNETCDF%");
    $b->value(0);
    push @fps, $b;
$a = PathParameter->new();
    $a->id("netcdf");
    $a->description("Path to the NetCDF directory");
    $a->placeholder("%netcdfPath%");
#    $a->markerFile("netcdf.h\$");
#    $a->relativeLocation("../");
    $a->autoDetect(0);
    $a->dependencyFlagID("withNETCDF");
    $a->dependencyDisabledValue("");
    push @pps, $a;

$c = ConfigFilePair->new();
    $c->inFile("ExtLibs.vsprops.input");
    $c->outFile("ExtLibs.vsprops");
    push @cfps, $c;

Configure("MegaMol(TM) Plugin Configuration for Windows", ".megamol.plg.win.cache", \@pps, \@fps, \@cfps, \@sps);

