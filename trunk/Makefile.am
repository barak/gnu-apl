
ACLOCAL_AMFLAGS = -I m4

APL_DIRS	= workspaces wslib3 wslib4 wslib5
SUBDIRS		= $(APL_DIRS)
SUBDIRS		+= build doc erlang gnu-apl.d HOWTOs
SUBDIRS		+= src support-files tools websock
DIST_SUBDIRS	= $(SUBDIRS) rpm debian

dist_noinst_SCRIPTS = buildtag.sh

dist_doc_DATA = README					\
		README-1-prerequisites			\
		README-2-configure			\
		README-3-keyboard			\
		README-4-compliance			\
		README-5-WINDOWS			\
		README-6-porting			\
		README-7-more-info			\
		README-8-parallel			\
		README-9-post-installation		\
		README-10-python			\
		README-11-bogus-compiler-warnings	\
		README-12-vpath-builds			\
		README-13-build-pitfalls

nobase_dist_sysconf_DATA = gnu-apl.d/preferences

dist_noinst_DATA = Doxyfile Makefile.incl buildtag.hh config.h

include Makefile.incl

all: makefile.h
makefile.h: Makefile

.PHONY: check_sources
check_sources: tools/check_src
	tools/check_src

buildtag.hh: Makefile buildtag.sh
	. ${top_srcdir}/buildtag.sh
	touch $@

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
	make -C rpm $@
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
	cd debian_tmp/$(distdir) ; dpkg-buildpackage -j4
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
	rm -f apl-$(VERSION)/makefile.h
	rm -f apl-$(VERSION)/buildtag.hh
	cp -a apl-$(VERSION)/* ../savannah-repo-apl/trunk/
	echo "exported from SVN version " `svnversion` > \
    ../savannah-repo-apl/trunk/exported_from
	rm -Rf apl-$(VERSION)
	cd ../savannah-repo-apl ; svn add . --force -q ; git add --update

# build all packages and SYNC
#
.PHONY:	EXPO
EXPO:
	tools/check_src
	svn -m "auto-commit by make EXPO, local SVN: `svnversion`" commit
	svn update
	. ./buildtag.sh
	svn -m "auto-commit by make EXPO" commit
	./configure --disable-maintainer-mode
	make all
	make dist
	make DEB
#	make RPM
	make SYNC

# EXPO1 is EXPO without updating/committing from/to SVN
.PHONY:	EXPO1
EXPO1:
	tools/check_src
	. buildtag.sh
	./configure --disable-maintainer-mode
	make
	make dist
	make DEB
	make RPM

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
	make -C src $@
	make -C wslib5 $@

makefile.h: Makefile
	@echo creating a fresh $@...
	@echo "/* this makefile.h exports some paths known by"    > $@
	@echo "   Makefile. See chapter 20.5 of the autoconf"    >> $@
	@echo "   manual (keyword: "dedicated header file")."    >> $@
	@echo " */"                                              >> $@
	@echo "#ifndef __makefile_h_DEFINED__"                   > $@
	@echo "# define __makefile_h_DEFINED__"                  >> $@
	@echo "# define Makefile__bindir     STR(${bindir})"     >> $@
	@echo "# define Makefile__docdir     STR(${docdir})"     >> $@
	@echo "# define Makefile__sysconfdir STR(${sysconfdir})" >> $@
	@echo "# define Makefile__pkglibdir  STR(${pkglibdir})"  >> $@
	@echo "# define Makefile__localedir  STR(${localedir})"  >> $@
	@echo "# define Makefile__srcdir     STR(${abs_srcdir})" >> $@
	@echo "# define Makefile__host_os    STR(${host_os})"    >> $@
	@echo "#endif  // __makefile_h_DEFINED__"                >> $@
	@echo ""                                                 >> $@

