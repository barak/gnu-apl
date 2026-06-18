
# noinst_PROGRAMS will be compiled (in this subdirectory 'tools')
# but will not be installed (in e.g. /usr/bin or /usr/local/bin).
# None of these tools are needed when cross-compiling for Windows.
#
if !apl_TARGET_MINGW
noinst_PROGRAMS = APL_keyboard       \
                  APL_keyboard_learn \
                  APL_keyboard_show  \
                  apl2html           \
                  check_command_def  \
                  filter_apl_lines   \
                  filter_apl_lines2  \
                  lines              \
                  phrase_gen         \
                  sockconst          \
                  TLV_server         \
                  unicodes_used

if BUILD_NCURSES_EMUL
noinst_PROGRAMS += ncurses_emul
endif
endif


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

dist_noinst_SCRIPTS = check_src plotlibs memory_benchmark.cc

APL_keyboard_SOURCES       = APL_keyboard.cc        APL_keyboard.def
APL_keyboard_learn_SOURCES = APL_keyboard_learn.cc  APL_keyboard.orig.def
APL_keyboard_show_SOURCES  = APL_keyboard_show.cc
apl2html_SOURCES           = apl2html.cc
check_command_def_SOURCES  = check_command_def.cc
filter_apl_lines_SOURCES   = filter_apl_lines.cc
filter_apl_lines2_SOURCES  = filter_apl_lines2.cc
lines_SOURCES              = lines.cc
ncurses_emul_SOURCES       = ncurses_emul.cc
ncurses_emul_LDADD         = -lncurses -ltinfo
TLV_server_SOURCES         = TLV_server.c
unicodes_used_SOURCES      = unicodes_used.cc

phrase_gen_SOURCES = phrase_gen.cc phrase_gen.def

gen: phrase_gen
	./phrase_gen > ../src/Prefix.def

gen2: phrase_gen
	./phrase_gen -t > ../src/Prefix.def

