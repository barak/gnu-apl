
dist_doc_DATA =	APL-on-Macintosh.pdf \
		APL-Communication-Cookbook.html \
		 Quad-GTK.html \
		glade-0.png \
		glade-1.png \
		glade-2.png \
		glade-3.png \
		glade-4.png

.asciidoc.html:
	if [ -z `which asciidoc` ] ; then touch -c $@ ; \
	else asciidoc -o $@ $< ; fi

.PRECIOUS: APL-Communication-Cookbook.html Quad-GTK.html glade-1.png \
    glade-2.png glade-3.png glade-4.png

