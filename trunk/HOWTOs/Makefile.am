
# HTML files generated from ASCIIDOC sources
#
ASCIIDOC = \
    APL-Communication-Cookbook.asciidoc \
    Parallel-APL.asciidoc               \
    Quad-GTK.asciidoc                   \
    GNU-APL-Memory-Management.asciidoc  \
    GNU-APL-Troubleshooting.asciidoc

HTML = \
    APL-Communication-Cookbook.html \
    Parallel-APL.html               \
    Quad-GTK.html                   \
    GNU-APL-Memory-Management.html  \
    GNU-APL-Troubleshooting.html

dist_doc_DATA = \
    $(HTML)              \
    $(ASCIIDOC)          \
    APL-on-Macintosh.pdf \
    Parallel-APL.pdf     \
    glade-0.png          \
    glade-1.png          \
    glade-2.png          \
    glade-3.png          \
    glade-4.png          \
    APL1.png             \
    APL2.png             \
    APL3.png             \
    APL4.png             \
    APL5.png

.asciidoc.html:
	if [ -z `which asciidoc` ] ; then touch $@ ;       \
	else asciidoc -a icons -o $@ $< ;                  \
             sed -i 's|<pre><code>|<pre>\n\n<code>|' $@ ;  \
             sed -i 's|</code></pre>|</code>\n\n</pre>|' $@ ; fi

.PRECIOUS: $(HTML) glade-1.png glade-2.png glade-3.png glade-4.png

# convert all .bmp files in /tmp to png files in current directory
.PHONY: PNGs
PNGs:
	for b in /tmp/*.bmp; do p=`basename "$$b" .bmp`; \
		convert "$$b" "./$$p.png"; done

