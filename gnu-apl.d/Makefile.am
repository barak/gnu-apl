
gnuapldir = $(sysconfdir)/gnu-apl.d
dist_noinst_DATA = preferences.in
dist_gnuapl_DATA = keyboard1.txt parallel_thresholds

# preferences is built by 'make all', but installed by install-data-local
# below, since the user may have adapted the installed one
noinst_DATA = preferences

# preferences is made here rather than by configure, so that the
# installation directories in it are fully expanded
preferences: preferences.in Makefile
	$(AM_V_GEN)$(SED) -e 's|[@]pkglibdir[@]|$(pkglibdir)|g'   \
	                  -e 's|[@]sysconfdir[@]|$(sysconfdir)|g' \
	                  $(srcdir)/preferences.in > $@

CLEANFILES = preferences

# install preferences only if there is none yet. An existing preferences file
# (most likely adapted by the user) is never overwritten: if it differs from
# the new default, then the new default is installed as preferences.new.
#
install-data-local: preferences
	@$(MKDIR_P) "$(DESTDIR)$(gnuapldir)"
	@f="$(DESTDIR)$(gnuapldir)/preferences";                             \
	if test ! -f "$$f"; then                                             \
	   echo " $(INSTALL_DATA) preferences '$$f'";                        \
	   $(INSTALL_DATA) preferences "$$f" || exit 1;                      \
	elif cmp -s preferences "$$f"; then                                  \
	   echo " $$f: unchanged";                                           \
	else                                                                 \
	   echo " $(INSTALL_DATA) preferences '$$f.new'";                    \
	   $(INSTALL_DATA) preferences "$$f.new" || exit 1;                  \
	   echo "NOTE: kept your existing $$f";                              \
	   echo "      The new default preferences were installed as $$f.new"; \
	fi

# remove preferences only if the user has not modified it
#
uninstall-local:
	@f="$(DESTDIR)$(gnuapldir)/preferences";                             \
	rm -f "$$f.new";                                                     \
	if cmp -s preferences "$$f"; then                                    \
	   echo " rm -f '$$f'"; rm -f "$$f";                                 \
	elif test -f "$$f"; then                                             \
	   echo "NOTE: $$f was modified and was not removed";                \
	fi
