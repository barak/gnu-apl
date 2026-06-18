
# vim: et:ts=4:sw=4

ACLOCAL_AMFLAGS = -I m4

APL_DIRS	= workspaces wslib3 wslib4 wslib5
SUBDIRS		= $(APL_DIRS)
SUBDIRS		+= build Claude doc erlang gnu-apl.d HOWTOs
SUBDIRS		+= src support-files tools websock
DIST_SUBDIRS	= $(SUBDIRS) rpm debian

dist_doc_DATA = 							\
		README								\
		README-1-prerequisites				\
		README-2-configure					\
		README-3-keyboard					\
		README-4-compliance					\
		README-5-WINDOWS					\
		README-6-porting					\
		README-7-more-info					\
		README-8-parallel					\
		README-9-post-installation			\
		README-10-python					\
		README-11-bogus-compiler-warnings	\
		README-12-vpath-builds				\
		README-13-build-pitfalls

nobase_dist_sysconf_DATA = gnu-apl.d/preferences

dist_noinst_DATA =				\
		Doxyfile				\
		Makefile.incl			\
		config.h				\
		dynamic_lookup-11.patch	\
		aclocal.patch

# wslib1 and wslib2 are reserved library directories with no shipped
# content of their own (see doc/apl.texi, "library-root"), so unlike
# wslib3/wslib4/wslib5 they have no Makefile of their own to install
# files from. Create them as empty directories so that a fresh install
# is recognized as a valid library-root (which requires wslib1 to exist).
#
install-data-hook:
	$(MKDIR_P) $(DESTDIR)$(pkglibdir)/wslib1
	$(MKDIR_P) $(DESTDIR)$(pkglibdir)/wslib2

include Makefile.incl

.PHONY: check_sources
check_sources: tools/check_src tools/check_command_def
	tools/check_src
	tools/check_command_def

# produce doxygen documentation
#
.PHONY:	DOXY
DOXY:
	doxygen

# build source and binary RPMs
#
.PHONY:	RPM
RPM:	DIST all
	rm -f rpm/rpm-tmp.*
	$(MAKE) -C rpm $@
	mkdir -p ftp-upload
	cp rpm/SRPMS/$(PACKAGE)-$(PACKAGE_VERSION)-0.src.rpm  ftp-upload/
	cp rpm/RPMS/*/$(PACKAGE)-$(PACKAGE_VERSION)-0.*.rpm   ftp-upload/

# build source and binary Debian packages
#
.PHONY:	DEB
DEB:	DIST
	rm -Rf debian_tmp
	mkdir debian_tmp
	cp $(distdir).tar.gz \
           debian_tmp/$(PACKAGE)_$(PACKAGE_VERSION).orig.tar.gz
	cd debian_tmp ; tar xf $(PACKAGE)_$(PACKAGE_VERSION).orig.tar.gz
	cp doc/apl.1 debian_tmp/$(distdir)/debian/manpage.1
	cd debian_tmp/$(distdir) ; MAKEFLAGS="" dpkg-buildpackage -j10
	mkdir -p ftp-upload
	cp debian_tmp/$(PACKAGE)_$(PACKAGE_VERSION)-1_*.deb          ftp-upload/
	cp debian_tmp/$(PACKAGE)_$(PACKAGE_VERSION)-1.debian.tar.xz  ftp-upload/

# update from SVN repository and ./configure
#
.PHONY:	SVNUP
SVNUP:
	svn update
	@if [ -x ./config.status ]; then ./config.status ; \
	else ./configure --disable-maintainer-mode ; fi

# a shortcut to export changes to ../savannah-repo-apl (which is supposed to be
# a checkout of the GNU APL repository at Savannah
#
.PHONY:	SYNC
SYNC:	DIST
	rm -Rf apl-$(VERSION)
	tar xvzf apl-$(VERSION).tar.gz
	cp -a apl-$(VERSION)/* ../savannah-repo-apl/trunk/
	echo "exported from SVN version " `svnversion` > \
    ../savannah-repo-apl/trunk/exported_from
	rm -Rf apl-$(VERSION)
	cp apl-$(VERSION).tar.gz ../savannah-repo-apl/
	cd ../savannah-repo-apl ; svn add . --force -q
	cd ../savannah-repo-apl ; git add .
	cd ../savannah-repo-apl ; git commit -m "sync from SVN"

# build all packages and SYNC the repos
#
.PHONY:	EXPO
EXPO:
	tools/check_src
	tools/check_command_def
	svn -m "auto-commit by make EXPO, local SVN: `svnversion`" commit
	svn update
	svn -m "auto-commit by make EXPO" commit
	./configure --disable-maintainer-mode
	$(MAKE) -j10 all
	$(MAKE) -j10 dist
	$(MAKE) -j10 DEB
#	$(MAKE) -j10 RPM
	$(MAKE) -j10 SYNC

# EXPO1 is EXPO without updating/committing from/to SVN
.PHONY:	EXPO1
EXPO1:
	tools/check_src
	tools/check_command_def
	./configure --disable-maintainer-mode
	$(MAKE) -j10
	$(MAKE) -j10 dist
	$(MAKE) -j10 DEB
	$(MAKE) -j10 RPM

# Stage a DESTDIR install tree containing only the platform-independent
# library content (workspaces, wslib1-5, the default preferences file) -
# i.e. everything that a Windows install needs *except* apl.exe itself,
# which must be cross-compiled separately (e.g. with MinGW) and is NOT
# produced by this (Linux) build. Place that versioned binary
# (apl-VERSION.exe, produced by 'make -C src windows-release') into
# support-files/windows-installer/ before running 'make windows-installer'.
#
.PHONY: windows-installer-stage
windows-installer-stage:
	rm -rf support-files/windows-installer/stage
	$(MAKE) -C workspaces install DESTDIR=$(abs_top_builddir)/support-files/windows-installer/stage
	$(MAKE) -C wslib3     install DESTDIR=$(abs_top_builddir)/support-files/windows-installer/stage
	$(MAKE) -C wslib4     install DESTDIR=$(abs_top_builddir)/support-files/windows-installer/stage
	$(MAKE) -C wslib5     install DESTDIR=$(abs_top_builddir)/support-files/windows-installer/stage
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib1
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib2
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib6
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib7
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib8
	$(MKDIR_P) support-files/windows-installer/stage$(pkglibdir)/wslib9
	$(MKDIR_P) support-files/windows-installer/stage$(sysconfdir)
	$(INSTALL_DATA) gnu-apl.d/preferences support-files/windows-installer/stage$(sysconfdir)/
	sed -i 's/^  LIBREF-\([345]\) /# LIBREF-\1 /' \
	    support-files/windows-installer/stage$(sysconfdir)/preferences

# Build the full Windows installer (apl-VERSION.exe + font + library
# directory structure) from the staged tree above.  Requires the "nsis"
# package (makensis).  When called via 'make windows-release', the binary
# is copied automatically.  For a standalone invocation, place the versioned
# binary apl-VERSION.exe in support-files/windows-installer/ first.
#
.PHONY: windows-installer
windows-installer: windows-installer-stage
	VUS=`echo "$(VERSION)" | tr . _` && \
	cd support-files/windows-installer && \
	makensis -DAPL_VERSION=$(VERSION) -DAPL_VERSION_US=$$VUS \
	         GNU_APL_2_0_Setup.nsi

# Produce versioned release files for the GNU mirror: the standalone binary
# apl-VERSION.exe and the full installer GNU_APL_VERSION_US_Setup.exe, both
# deposited in the top-level directory.  Calls 'make -C src windows-release'
# to build and stage the binary, then 'make windows-installer' to run NSIS.
# Must be run in the MinGW build tree (configured with --with-mingw).
#
.PHONY: windows-release
windows-release:
	$(MAKE) -C src windows-release
	$(MAKE) windows-installer
	cp support-files/windows-installer/apl-$(VERSION).exe apl-$(VERSION).exe
	VUS=`echo "$(VERSION)" | tr . _` && \
	cp "support-files/windows-installer/GNU_APL_$${VUS}_Setup.exe" \
	   "GNU_APL_$${VUS}_Setup.exe"

Makefile.dist:
	echo "%:"		 > $@
	echo "rm -f Makefile"	>> $@
	echo "./configure"	>> $@
	echo "make $@"		>> $@
	echo 			>> $@

# perform 'make dist' and then add file './Makefile.dist' as 'Makefile' to
# the GNU APL project tar file created by 'make dist'.
#
# This is a trick to enable the building of make targets that do not need
# ./configure (i.e. help, develop, develop_lib, parallel, and parallel1)
# before ./configure is run. This avoids running ./configure if not needed
# or running ./configure twice (e.g. for make develop)
#
DIST:	dist Makefile.dist
	pwd
	mkdir -p tmp
	cd tmp; tar xzf ../apl-$(VERSION).tar.gz;		\
	cp -f ../Makefile.dist apl-$(VERSION)/Makefile;		\
	tar czf ../apl-$(VERSION).tar.gz apl-$(VERSION)
	rm -Rf tmp

DIST: check_sources

am--refresh:
	echo "making am--refresh"

EXTRA_DIST = config.rpath m4/ChangeLog

.PHONY: git-pull
git-pull:
	$(MAKE) -C src $@
	$(MAKE) -C wslib5 $@
	git clone https://git.code.sf.net/p/gnu-apl-library/code /tmp/DALY
	cp /tmp/DALY/doc/DALY/*     doc/DALY/
	cp /tmp/DALY/wslib3/DALY/*  wslib3/DALY/
	cp /tmp/DALY/wslib5/DALY/*  wslib5/DALY/
 

