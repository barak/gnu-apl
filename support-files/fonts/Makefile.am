
fontsdir = $(datadir)/fonts/truetype/apl

dist_fonts_DATA = GNU_APL.ttf COPYRIGHT_NOTICE

# Windows users do not run "make install"; ship the installer sources in the
# distribution tarball without installing them on Linux. GNU_APL_Font_Setup.exe
# is a build product (see the "installer" target below) and is not shipped.
EXTRA_DIST = install_GNU_APL_font.bat GNU_APL_Font.nsi

install-data-hook:
	-fc-cache -f $(fontsdir)

# Builds the graphical Windows installer/uninstaller. Requires the "nsis"
# package (makensis). Not part of the default build since most build
# machines will not have it installed.
.PHONY: installer
installer: GNU_APL_Font.nsi GNU_APL.ttf COPYRIGHT_NOTICE
	makensis GNU_APL_Font.nsi

