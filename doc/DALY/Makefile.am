
dist_doc_DALY_DATA =    apl_kbd.texi            \
                        apl-library.info        \
                        apl-library.texi        \
                        apl-pkg-mgr.texi        \
                        aparse.texi             \
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
                        lpr.texi                \
                        pref.texi               \
                        prompt.texi             \
                        pw.texi                 \
                        seq.texi                \
                        stack.texi              \
                        stat.texi               \
                        utf8.texi               \
                        utl.texi                \
                        wblg.texi               \
                        wp.texi                 \
                        xml.texi

doc_DALYdir = $(pkglibdir)/doc/DALY

# NOTE: listing a file in info_TEXINFOS causes the corresponding .html file
# to be deleted on 'make clean' That is bad for people not having a
# $MAKEINFOHTML installed. We therefore leave info_TEXINFOS empty and instead
# install info stuff manually below.
#
info_TEXINFOS =

# the .texi files included by apl-library.texi
#
APL_LIBRARY_INCLUDES =                                          \
        apl_kbd.texi aparse.texi assert.texi cfg.texi date.texi \
        dom.texi export.texi finance.texi fmt.texi html.texi    \
        import.texi lex.texi lex1.texi cl.texi lpr.texi pw.texi \
        pref.texi prompt.texi seq.texi stack.texi stat.texi     \
        utf8.texi utl.texi wblg.texi wp.texi xml.texi           \
        apl-pkg-mgr.texi fdl-1.3.texi

# apl-library.info is shipped pre-built, like the manuals in ../, and
# rebuilt from its .texi sources only if ./configure has found makeinfo
# (see ../Makefile.am).
#
if apl_MAKEINFO

apl-library.info: apl-library.texi $(APL_LIBRARY_INCLUDES)
	$(MAKEINFO) -I $(srcdir) $< -o $@ --no-split

else !apl_MAKEINFO

apl-library.info: apl-library.texi $(APL_LIBRARY_INCLUDES)
	@if test -f $@ || test -f $(srcdir)/$@; then                    \
	    echo "WARNING: makeinfo not found: using the shipped (older) $@"; \
	 else echo "ERROR: $@ is missing and ./configure found no makeinfo"; \
	    exit 1; fi

endif !apl_MAKEINFO

# install apl-library.info from the build directory if it was rebuilt there
# (VPATH build), and otherwise the shipped one from $(srcdir).
#
install-info-am: apl-library.info
	@echo " $(MKDIR_P) $(DESTDIR)$(infodir)"
	@       $(MKDIR_P) "$(DESTDIR)$(infodir)" || exit 1
	@if test -f apl-library.info; then d=; else d="$(srcdir)/"; fi;  \
	 echo " $(INSTALL_DATA) $${d}apl-library.info $(DESTDIR)$(infodir)"; \
	 $(INSTALL_DATA) "$${d}apl-library.info" "$(DESTDIR)$(infodir)"

uninstall-info-am:
	@echo " rm -f $(DESTDIR)$(infodir)/apl-library.info"
	@       rm -f $(DESTDIR)$(infodir)/apl-library.info

