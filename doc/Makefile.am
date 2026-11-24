
# Bill Daly's APL library
SUBDIRS = DALY

# NOTE: listing a file in info_TEXINFOS causes the corresponding .html file
# to be deleted on 'make clean' That is bad for people not having a
# $MAKEINFOHTML installed. We therefore leave info_TEXINFOS empty and instead
# install info stuff manually below.
#
info_TEXINFOS = 

man1_MANS = apl.1

# info_TEXINFOS = fails for more than one .info file. We therefore install
# apl.info and libapl.info explicitly: from the build directory if they were
# rebuilt there (VPATH build), and otherwise the shipped ones from $(srcdir).
#
install-info-am: apl.info libapl.info
	@echo " $(MKDIR_P) $(DESTDIR)$(infodir)"
	@       $(MKDIR_P) "$(DESTDIR)$(infodir)" || exit 1
	@for f in apl.info libapl.info; do                               \
	    if test -f $$f; then d=; else d="$(srcdir)/"; fi;            \
	    echo " $(INSTALL_DATA) $$d$$f $(DESTDIR)$(infodir)";         \
	    $(INSTALL_DATA) "$$d$$f" "$(DESTDIR)$(infodir)" || exit 1;   \
	 done

uninstall-info-am:
	 @echo " rm -f $(DESTDIR)$(infodir)/apl.info"
	 @       rm -f $(DESTDIR)$(infodir)/apl.info
	 @echo " rm -f $(DESTDIR)$(infodir)/libapl.info"
	 @       rm -f $(DESTDIR)$(infodir)/libapl.info

# The manuals in .info and .html format are shipped pre-built, so that
# building GNU APL does not need makeinfo. They are rebuilt from their .texi
# sources only if ./configure has found makeinfo; a makeinfo error then stops
# the build. Without makeinfo, the shipped files are used (with a warning if
# the .texi source is newer), and a missing file is an error.
#
if apl_MAKEINFO

# apl.texi in HTML format
#
apl.html:   apl.texi apl-info.css
	$(MAKEINFOHTML) $< -o $@ --no-split --css-include=$(srcdir)/apl-info.css

# apl.texi in info format
#
apl.info:   apl.texi
	$(MAKEINFO) $< -o $@ --no-split

# libapl.texi in HTML format
#
libapl.html: libapl.texi
	$(MAKEINFOHTML) $< -o $@ --no-split

# libapl.texi in info format
#
libapl.info: libapl.texi
	$(MAKEINFO) $< -o $@ --no-split

else !apl_MAKEINFO

SHIPPED_MANUAL = if test -f $@ || test -f $(srcdir)/$@; then                 \
   echo "WARNING: makeinfo not found: using the shipped (older) $@";         \
   else echo "ERROR: $@ is missing and ./configure found no makeinfo";       \
   exit 1; fi

apl.html apl.info: apl.texi
	@$(SHIPPED_MANUAL)

libapl.html libapl.info: libapl.texi
	@$(SHIPPED_MANUAL)

endif !apl_MAKEINFO


# NOTE that the .html version of aspl.text refers
# to some of the files in dist_doc_DATA.
#
dist_doc_DATA =				\
	apl.1					\
	apl.texi				\
	apl.info				\
	apl-info.css			\
	apl.html				\
	libapl.texi				\
	libapl.info				\
	PLOT_circle.png			\
	PLOT_zigzag.png			\
    MX_distributions.png	\
	libapl.html

