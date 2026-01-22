
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
# libapl.info explicitly
#
install-info-am:
	@echo " $(MKDIR_P) $(DESTDIR)$(infodir)"
	@       $(MKDIR_P) "$(DESTDIR)$(infodir)" || exit 1
	@echo " $(INSTALL_DATA) $(srcdir)/apl.info  $(srcdir)/libapl.info $(DESTDIR)$(infodir)"
	@       $(INSTALL_DATA) $(srcdir)/apl.info  $(srcdir)/libapl.info $(DESTDIR)$(infodir)

uninstall-info-am:
	 @echo " rm -f $(DESTDIR)$(infodir)/apl.info"
	 @       rm -f $(DESTDIR)$(infodir)/apl.info
	 @echo " rm -f $(DESTDIR)$(infodir)/libapl.info"
	 @       rm -f $(DESTDIR)$(infodir)/libapl.info

# apl.texi in HTML format
#
apl.html:   apl.texi apl-info.css
	@$(MAKEINFOHTML) $< -o $@ --no-split --css-include=apl-info.css || touch $@

# apl.texi in info format
#
apl.info:   apl.texi
	@$(MAKEINFO) $< -o $@ --no-split || touch $@

# libapl.texi in HTML format
#
libapl.html: libapl.texi
	$(MAKEINFOHTML) $< -o $@ --no-split || touch $@

# libapl.texi in info format
#
libapl.info: libapl.texi
	$(MAKEINFO) $< -o $@ --no-split || touch $@


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

