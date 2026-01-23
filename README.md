# mport

MidnightBSD Package Manager

## Building

This project uses **BSD make** (bmake), not GNU make. On MidnightBSD, `make` is typically bmake.

On Linux, install bmake and run:

```sh
bmake
```

## Requirements

Recent versions will run on MidnightBSD 3.0 and higher.

Depends on:

* sqlite3
* libarchive
* bzip2
* lzma
* ucl

Depends on included libs:
external/tllist

Versions prior to 2.1.0 also depended on

* libdispatch
* Blocksruntime

## Backward compatibility

The last version that works with MidnightBSD 1.2.8 or lower is mport 2.1.4. (lack of libucl)  

There was a breaking change in 2.1.6 in libmport with respect to mport_install and mport_install_primative use.

There was a breaking change in 2.6.0 in libmport which changed the mport_init function to use mportVerbosity rather than a boolean for quiet mode.

mportVerbosity has three values currently:
MPORT_VQUIET, MPORT_VNORMAL, MPORT_VVERBOSE

There's also the new mport_verbosity function which translates quiet and verbose flags into the right value.

The last version that works with MidnightBSD 2.x is mport 2.6.5.

## Using mport

In addition to the man page, you can also look at the BSD Magazine article on mport in the Feb 2012 issue.
<https://ia902902.us.archive.org/6/items/bsdmagazine/BSD_02_2012.pdf>

### Quick and Dirty

Installing a package named xorg:

```sh
mport install xorg
```

Deleting the xorg meta-package:

```sh
mport delete xorg
```

Fetching the latest index

```sh
mport index
```

Upgrading all packages

```sh
mport upgrade
```

Update a specific package (and its dependencies)

```sh
mport update xorg
```

### Installing from package file

For example, installing a vim package that is already built locally

`/usr/libexec/mport.install /usr/mports/Packages/amd64/All/vim-8.2.3394.mport` 

### Getting info on an installed package

```sh
mport info gmake

gmake-4.4.1
Name            : gmake
Version         : 4.4.1
Latest          : 4.4.1
Licenses        : gpl3
Origin          : devel/gmake
Flavor          : 
OS              : 4.0
CPE             : cpe:2.3:a:gnu:make:4.4.1:::::midnightbsd4:x64:0
PURL            : pkg:generic/gmake@4.4.1?arch=amd64&distro=midnightbsd-4.0
Locked          : no
Prime           : yes
Shared library  : no
Deprecated      : no
Expiration Date : 
Install Date    : Sat Jan 17 16:33:59 2026
Comment         : GNU version of 'make' utility
Annotations     :
                       MidnightBSD_version:        �␦
                       build_timestamp:        2026-01-17
                       bundle_format_version:        6
                       os_release:        4.0
Options         : 
Type            : Application
Flat Size       : 2.1 MiB
Description     :

```

### Security-related commands

```sh
mport audit
```

Displays vulnerable packages based on their CPE identifiers using the NVD data provided by https://sec.midnightbsd.org

```sh
mport audit -r
```

Prints out vulnerable packages and a list of packages depending on that one.

```sh
mport -q audit
```

Prints out vulnerable package name and version with no descriptions or details

```sh
mport cpe
```

Lists all CPE info on installed packages

```sh
mport verify
```

Runs a checksum on all installed files from packages against data from time of installation to see if files have been modified.

### Known Bugs

Old versions of mport had a bug that would prevent it working over a serial connection such as during a bhyve installation.  A workaround is to ssh into the box to do installs.  This is known
to impact MidnightBSD 1.2.x and lower.  It was fixed.
