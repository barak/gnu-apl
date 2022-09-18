
SUBDIRS = APLComponentFiles DALY iso-apl-cf

dist_apl5_DATA =	HTML.apl	\
			FILE_IO.apl	\
			FILE_IO1.apl	\
			SQL.apl

apl5dir = $(pkglibdir)/wslib5

.PHONY: git-pull
git-pull:
	make -C APLComponentFiles $@
	make -C iso-apl-cf $@

