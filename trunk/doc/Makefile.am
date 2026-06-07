
# Bill Daly's APL library
SUBDIRS = DALY

# NOTE: listing a file in info_TEXINFOS causes the corresponding .html file
# to be deleted on 'make clean' That is bad for people not having a
# $MAKEINFOHTML installed. We therefore leave info_TEXINFOS empty and instead
# install info stuff manually below.
#
info_TEXINFOS = apl.texi libapl.texi

man1_MANS = apl.1
HTMLFILES = apl.html libapl.html
MAKEINFOFLAGS = --no-split

apl.html: apl.texi apl-info.css
	$(MAKEINFO) --html $(MAKEINFOFLAGS) --css-include=apl-info.css $<
libapl.html: libapl.texi apl-info.css
	$(MAKEINFO) --html $(MAKEINFOFLAGS) --css-include=apl-info.css $<

CLEANFILES = $(HTMLFILES) apl.info libapl.info

nodist_doc_DATA = $(HTMLFILES)

dist_doc_DATA =		\
	PLOT_circle.png	\
	PLOT_zigzag.png	\
	MX_distributions.png
