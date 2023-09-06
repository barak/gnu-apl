
# HTML files generated from ASCIIDOC sources
#
ASCIIDOC = \
    APL-Communication-Cookbook.asciidoc \
    Parallel-APL.asciidoc               \
    Quad-GTK.asciidoc                   \
    GNU-APL-Memory-Management.asciidoc  \
    GNU-APL-Troubleshooting.asciidoc    \
    GNU-APL-Designers-Guide.asciidoc    \
    GNU-APL-Performance.asciidoc        \
    LApack-primer.asciidoc

HTML = \
    APL-Communication-Cookbook.html \
    Parallel-APL.html               \
    Quad-GTK.html                   \
    GNU-APL-Memory-Management.html  \
    GNU-APL-Troubleshooting.html    \
    GNU-APL-Designers-Guide.html    \
    GNU-APL-Performance.html        \
    LApack-primer.html

# Makefile.am produces all .pdf and .html files, but not all of them
# shall be distributed...
#
dist_doc_DATA = \
    $(HTML)              \
    $(ASCIIDOC)          \
    APL-on-Macintosh.pdf \
    LApack-primer.pdf    \
    Parallel-APL.pdf     \
    glade-0.png          \
    glade-1.png          \
    glade-2.png          \
    glade-3.png          \
    glade-4.png          \
    glade-5.png          \
    glade-6.png          \
    glade-7.png          \
    glade-8.png          \
    glade-9.png          \
    glade-10.png         \
    glade-11.png         \
    glade-12.png         \
    APL1.png             \
    APL2.png             \
    APL3.png             \
    APL4.png             \
    APL5.png

# asciidoctor has more features that asciidoc, but ASCII line graphics
# (used a  lot in our documents) look well in asciidoc but pretty bad
# in asciidoctor.
#
ADOC = asciidoc                # use asciidoc
# ADOC = asciidoctor           # use asciidoctor

# internal documentation (build but do not distribute)
#
all: LApack-primer.pdf

# use asciidoc (if installed) to convert .asciidoc to .html
# The .html has a stupid <noscript> tag that needs to be removed
# in order to properly display the Table of Contents.
.asciidoc.html:
	@if [ -z `which asciidoc` ] ; then touch $@ ;        \
	else asciidoc -a icons -o $@ $< ;                    \
        sed -i 's|Cpp|C++|g'                        $@ ; \
        sed -i 's|<pre><code>|<pre>\n\n<code>|'     $@ ; \
        sed -i 's|</code></pre>|</code>\n\n</pre>|' $@ ; \
	    grep -v -F "<noscript>" $@           > /tmp/$@ ; \
        mv -f /tmp/$@                               $@ ; \
    fi

# use LibreOffic Writer (if installed) to convert .html to .pdf
#
.html.pdf:
	@if [ -z `which lowriter` ] ; then touch $@  ; \
	else lowriter --headless --convert-to pdf $< ; \
	fi

.PRECIOUS: $(HTML) glade-1.png glade-2.png glade-3.png glade-4.png

# convert all .bmp files in /tmp to png files in current directory
.PHONY: PNGs
PNGs:
	for b in /tmp/*.bmp; do p=`basename "$$b" .bmp`; \
		convert "$$b" "./$$p.png"; done

