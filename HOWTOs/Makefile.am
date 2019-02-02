
# HTML files generated from ASCIIDOC sources
#
HTML = APL-Communication-Cookbook.html \
       Quad-GTK.html                   \
       GNU-APL-Memory-Management.html

dist_doc_DATA =	APL-on-Macintosh.pdf	\
		$(HTML)			\
		glade-0.png		\
		glade-1.png		\
		glade-2.png		\
		glade-3.png		\
		glade-4.png

.asciidoc.html:
	if [ -z `which asciidoc` ] ; then touch $@ ;       \
	else asciidoc -a icons -o $@ $< ;                  \
             sed -i 's|<pre><code>|<pre>\n\n<code>|' $@ ;  \
             sed -i 's|</code></pre>|</code>\n\n</pre>|' $@ ; fi

.PRECIOUS: $(HTML) glade-1.png glade-2.png glade-3.png glade-4.png

