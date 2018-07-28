
dist_doc_DATA =	APL-on-Macintosh.pdf \
		APL-Communication-Cookbook.html

.asciidoc.html:
	if [ -z `which asciidoc` ] ; then touch -c $@ ; \
	else asciidoc -o $@ $< ; fi

