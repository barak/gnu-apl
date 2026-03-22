
TAR_FILENAME = $(PACKAGE)-$(VERSION).tar.gz
RPM_DIRS = BUILD BUILDROOT SOURCES RPMS SPECS SRPMS
RPM_ARGS = -ba SPECS/apl.spec --buildroot $(abs_top_builddir)/rpm/BUILDROOT

# run 'sudo make install' befor this:
#
.PHONY: RPM
RPM:
	rm -Rf $(RPM_DIRS)
	mkdir  $(RPM_DIRS)
	mkdir -p BUILDROOT/usr/lib64/apl/
	cp apl.spec SPECS/
	cp ../$(TAR_FILENAME) SOURCES/
	cp /usr/lib64/apl/*.la BUILDROOT/usr/lib64/apl/
	cp -a /usr/lib64/apl/workspaces BUILDROOT/usr/lib64/apl/
	rpmbuild $(RPM_ARGS)

clean-local:
	rm -Rf BUILD BUILDROOT SOURCES RPMS SPECS SRPMS

