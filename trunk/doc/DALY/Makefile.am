
dist_doc_DALY_DATA =    apl_kbd.texi            \
                        apl-library.info        \
                        apl-library.texi        \
                        apl-pkg-mgr.texi        \
                        arg_parser.texi         \
                        assert.texi             \
                        cfg.texi                \
                        cl.texi                 \
                        date.texi               \
                        dom.texi                \
                        export.texi             \
                        fdl-1.3.texi            \
                        finance.texi            \
                        fmt.texi                \
                        html.texi               \
                        import.texi             \
                        lex1.texi               \
                        lex.texi                \
                        lpr.texi

doc_DALYdir = $(pkglibdir)/doc/DALY

 NOTE: listing a file in info_TEXINFOS causes the corresponding .html file
# to be deleted on 'make clean' That is bad for people not having a
# $MAKEINFOHTML installed. We therefore leave info_TEXINFOS empty and instead
# install info stuff manually below.
#
info_TEXINFOS =

install-info-am:
	@echo " $(MKDIR_P) $(DESTDIR)$(infodir)"
	@       $(MKDIR_P) "$(DESTDIR)$(infodir)" || exit 1
	@echo " $(INSTALL_DATA) $(srcdir)/apl-library.info   $(DESTDIR)$(infodir)"
	@       $(INSTALL_DATA) $(srcdir)/apl-library.info  $(DESTDIR)$(infodir)

uninstall-info-am:
	@echo " rm -f $(DESTDIR)$(infodir)/apl.info"
	@       rm -f $(DESTDIR)$(infodir)/apl.info
	@echo " rm -f $(DESTDIR)$(infodir)/libapl.info"
	@       rm -f $(DESTDIR)$(infodir)/libapl.info

