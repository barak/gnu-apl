
# noinst_PROGRAMS will be compiled (in this subdirectory 'tools')
# but not installed (in e.g. /usr/bind or /usr/local/bin).
#
noinst_PROGRAMS =	APL_keyboard		\
			APL_keyboard_learn	\
			APL_keyboard_show	\
			apl2html		\
			phrase_gen		\
			sockconst		\
			TLV_server		\
			ncurses_emul


# fails on cygwin, therefore moved to dist_noinst_SCRIPTS
#
# if HAVE_RDTSC
# if HAVE_AFFINITY_NP
# noinst_PROGRAMS += memory_benchmark
# memory_benchmark_SOURCES = memory_benchmark.cc
# memory_benchmark_LDADD = -lpthread
# memory_benchmark_CXXFLAGS = -Wall
# endif
# endif

dist_noinst_SCRIPTS = check_src memory_benchmark.cc

APL_keyboard_SOURCES       = APL_keyboard.cc        APL_keyboard.def
APL_keyboard_learn_SOURCES = APL_keyboard_learn.cc  APL_keyboard.orig.def
APL_keyboard_show_SOURCES  = APL_keyboard_show.cc
apl2html_SOURCES           = apl2html.cc
TLV_server_SOURCES         = TLV_server.c
ncurses_emul_SOURCES       = ncurses_emul.cc
ncurses_emul_LIBS          = -lncurses -ltinfo

phrase_gen_SOURCES = phrase_gen.cc phrase_gen.def

gen:	phrase_gen
	./phrase_gen > ../src/Prefix.def

