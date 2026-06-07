info_TEXINFOS = apl-library.texi
MAKEINFOFLAGS = --no-split

apl-library.html: apl-library.texi
	$(MAKEINFO) --html $(MAKEINFOFLAGS) $<

CLEANFILES = apl-library.html apl-library.info

nodist_doc_DATA = apl-library.html
