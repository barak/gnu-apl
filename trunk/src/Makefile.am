
# NOTE: without 'no-dependencies' make target distclean won't work.
# AUTOMAKE_OPTIONS = no-dependencies

EXTRA_DIRS =
if ! WANT_ANDROID
EXTRA_DIRS += APs emacs_mode sql workspaces
endif
SUBDIRS = native $(EXTRA_DIRS)

all: ../makefile.h

# depending on HAVE_GTK3 we want either Plot_gtk.cc or Plot_xcb.cc compiled,
# but keep both source files in the tar file (hence EXTRA_apl_SOURCES).
#
if HAVE_GTK3
SUBDIRS += Gtk
   plot_SOURCES      = Plot_gtk.cc
   plot_CXXFLAGS     = -Wno-parentheses
   EXTRA_apl_SOURCES = Plot_xcb.cc
else
   plot_SOURCES      = Plot_xcb.cc
   EXTRA_apl_SOURCES = Plot_gtk.cc
endif

dist_noinst_DATA = valgrind.supp
DIST_SUBDIRS = $(SUBDIRS) testcases

datadir = @datadir@
localedir = $(datadir)/locale
host_os = @host_os@

# common means common to the apl and libapl builds
#
common_CXXFLAGS = -Wall $(plot_CXXFLAGS) -I sql -I $(abs_top_srcdir) $(CXX_WERROR)
common_CXXFLAGS += $(GTK_CFLAGS) $(GTK_LIBS)
common_CXXFLAGS += $(PYTHON_CFLAGS)
common_LDFLAGS  = -export-dynamic $(GTK_LDFLAGS)

common_SOURCES  = $(plot_SOURCES) 
common_SOURCES +=						../buildtag.hh
common_SOURCES +=						../config.h
common_SOURCES +=						APL_Float_as_class.hh
common_SOURCES +=						APL_types.hh
common_SOURCES += Archive.cc					Archive.hh
common_SOURCES +=						ArrayIterator.hh
common_SOURCES += Assert.cc					Assert.hh
common_SOURCES += Avec.cc		Avec.def		Avec.hh
common_SOURCES += Backtrace.cc					Backtrace.hh
common_SOURCES += Bif_F12_COMMA.cc				Bif_F12_COMMA.hh
common_SOURCES +=Bif_F12_DOMINO.cc				Bif_F12_DOMINO.hh
common_SOURCES +=Bif_F12_INDEX_OF.cc				Bif_F12_INDEX_OF.hh
common_SOURCES +=Bif_F12_INTERVAL_INDEX.cc			Bif_F12_INTERVAL_INDEX.hh
common_SOURCES +=Bif_F12_FORMAT.cc				Bif_F12_FORMAT.hh
common_SOURCES +=Bif_F12_PARTITION_PICK.cc			Bif_F12_PARTITION_PICK.hh
common_SOURCES += Bif_F12_SORT.cc				Bif_F12_SORT.hh
common_SOURCES += Bif_F12_TAKE_DROP.cc				Bif_F12_TAKE_DROP.hh
common_SOURCES += Bif_OPER1_COMMUTE.cc				Bif_OPER1_COMMUTE.hh
common_SOURCES += Bif_OPER1_EACH.cc				Bif_OPER1_EACH.hh
common_SOURCES += Bif_OPER2_POWER.cc				Bif_OPER2_POWER.hh
common_SOURCES +=  Bif_OPER2_INNER.cc				Bif_OPER2_INNER.hh
common_SOURCES += Bif_OPER2_OUTER.cc				Bif_OPER2_OUTER.hh
common_SOURCES += Bif_OPER2_RANK.cc				Bif_OPER2_RANK.hh
common_SOURCES += Bif_OPER1_REDUCE.cc				Bif_OPER1_REDUCE.hh
common_SOURCES += Bif_OPER1_SCAN.cc				Bif_OPER1_SCAN.hh
common_SOURCES += CDR.cc					CDR.hh
common_SOURCES += Cell.cc		Cell.icc		Cell.hh
common_SOURCES += Doxy.cc					Doxy.hh
common_SOURCES += CharCell.cc					CharCell.hh
common_SOURCES += Command.cc		Command.def		Command.hh
common_SOURCES += Common.cc					Common.hh
common_SOURCES += ComplexCell.cc				ComplexCell.hh
common_SOURCES += 						ConstCell_P.hh
common_SOURCES += DerivedFunction.cc				DerivedFunction.hh
common_SOURCES += DiffOut.cc					DiffOut.hh
common_SOURCES += DynamicObject.cc				DynamicObject.hh
common_SOURCES += Error.cc		Error.def		Error.hh
common_SOURCES +=						Error_macros.hh
common_SOURCES +=						ErrorCode.hh
common_SOURCES += Executable.cc					Executable.hh
common_SOURCES += FloatCell.cc					FloatCell.hh
common_SOURCES += Function.cc					Function.hh
common_SOURCES +=			Help.def
common_SOURCES +=						Heapsort.hh
common_SOURCES +=Id.cc			Id.def			Id.hh
common_SOURCES +=						IdEnums.hh
common_SOURCES += IndexExpr.cc					IndexExpr.hh
common_SOURCES += IndexIterator.cc				IndexIterator.hh
common_SOURCES += InputFile.cc					InputFile.hh
common_SOURCES += IntCell.cc					IntCell.hh
common_SOURCES += IO_Files.cc					IO_Files.hh
common_SOURCES += LApack.cc					LApack.hh
common_SOURCES += LibPaths.cc					LibPaths.hh
common_SOURCES += LineInput.cc					LineInput.hh
common_SOURCES += Logging.cc		Logging.def		Logging.hh
common_SOURCES += LvalCell.cc					LvalCell.hh
common_SOURCES += Malloc_hooks.cc
common_SOURCES += Nabla.cc					Nabla.hh
common_SOURCES += Macro.cc		Macro.def		Macro.hh
common_SOURCES += Missing_Libraries.cc
common_SOURCES += NamedObject.cc				NamedObject.hh
common_SOURCES += NativeFunction.cc				NativeFunction.hh
common_SOURCES += 						Native_interface.hh
common_SOURCES += NumericCell.cc	NumericCell.icc		NumericCell.hh
common_SOURCES += Output.cc					Output.hh
common_SOURCES += 						PJob.hh
common_SOURCES += Parallel.cc					Parallel.hh
common_SOURCES += Parser.cc					Parser.hh
common_SOURCES += Performance.cc	Performance.def		Performance.hh
common_SOURCES += PointerCell.cc				PointerCell.hh
common_SOURCES += Prefix.cc		Prefix.def		Prefix.hh
common_SOURCES += Plot_line_properties.cc			Plot_line_properties.hh
common_SOURCES += Plot_data.cc					Plot_data.hh
common_SOURCES += Plot_window_properties.cc			Plot_window_properties.hh
common_SOURCES += PrimitiveFunction.cc				PrimitiveFunction.hh
common_SOURCES += 						PrimitiveOperator.hh
common_SOURCES += PrintBuffer.cc				PrintBuffer.hh
common_SOURCES += 						PrintContext.hh
common_SOURCES += 						PrintOperator.hh
common_SOURCES += QuadFunction.cc				QuadFunction.hh
common_SOURCES += ProcessorID.cc				ProcessorID.hh
common_SOURCES += Quad_CR.cc		Quad_CR.def		Quad_CR.hh
common_SOURCES += Quad_DLX.cc					Quad_DLX.hh
common_SOURCES += Quad_FFT.cc					Quad_FFT.hh
common_SOURCES += Quad_FIO.cc		Quad_FIO.def		Quad_FIO.hh
common_SOURCES += Quad_FX.cc					Quad_FX.hh
common_SOURCES += Quad_GTK.cc					Quad_GTK.hh
common_SOURCES += Quad_JSON.cc					Quad_JSON.hh
common_SOURCES += Quad_MAP.cc					Quad_MAP.hh
common_SOURCES += Quad_PLOT.cc		Quad_PLOT.def		Quad_PLOT.hh
common_SOURCES += Quad_RE.cc					Quad_RE.hh
common_SOURCES += Quad_RL.cc					Quad_RL.hh
common_SOURCES += Quad_RVAL.cc					Quad_RVAL.hh
common_SOURCES += Quad_SQL.cc					Quad_SQL.hh
common_SOURCES += Quad_SVx.cc					Quad_SVx.hh
common_SOURCES += Quad_TF.cc					Quad_TF.hh
common_SOURCES += Quad_WA.cc					Quad_WA.hh
common_SOURCES += Quad_XML.cc					Quad_XML.hh
common_SOURCES += RealCell.cc					RealCell.hh
common_SOURCES += sbrk.cc
common_SOURCES += Shape.cc					Shape.hh
common_SOURCES += ScalarFunction.cc				ScalarFunction.hh
common_SOURCES += Security.cc		Security.def		Security.hh
common_SOURCES += StateIndicator.cc				StateIndicator.hh
common_SOURCES += Svar_DB.cc					Svar_DB.hh
common_SOURCES += Svar_record.cc				Svar_record.hh
common_SOURCES += tcp_signal.m4		Svar_signals.def	Svar_signals.hh
common_SOURCES += Symbol.cc					Symbol.hh
common_SOURCES += SymbolTable.cc				SymbolTable.hh
common_SOURCES += 			SystemLimits.def	SystemLimits.hh
common_SOURCES += SystemVariable.cc	SystemVariable.def	SystemVariable.hh
common_SOURCES += TabExpansion.cc				TabExpansion.hh
common_SOURCES += Thread_context.cc				Thread_context.hh
common_SOURCES += Token.cc		Token.def		Token.hh
common_SOURCES += 						TokenEnums.hh
common_SOURCES += Tokenizer.cc					Tokenizer.hh
common_SOURCES += UCS_string.cc					UCS_string.hh
common_SOURCES += UCS_string_vector.cc				UCS_string_vector.hh
common_SOURCES += 						Unicode.hh
common_SOURCES += UserFunction.cc				UserFunction.hh
common_SOURCES += UserFunction_header.cc			UserFunction_header.hh
common_SOURCES += UserPreferences.cc				UserPreferences.hh
common_SOURCES += UTF8_string.cc				UTF8_string.hh
common_SOURCES += Value.cc		Value.icc		Value.hh
common_SOURCES += 			Value_P.icc		Value_P.hh
common_SOURCES += ValueHistory.cc				ValueHistory.hh
common_SOURCES += Workspace.cc					Workspace.hh
common_SOURCES += static_Objects.cc				static_Objects.hh

if HAVE_LIBPCRE2_32
common_SOURCES += Regexp.cc					Regexp.hh
endif

if SQLITE3
common_CXXFLAGS += $(SQLITE3_CFLAGS)
common_LDFLAGS  += $(SQLITE3_LDFLAGS)
common_SOURCES += sql/SqliteArgListBuilder.cc			sql/SqliteArgListBuilder.hh
common_SOURCES += sql/SqliteConnection.cc			sql/SqliteConnection.hh
common_SOURCES += sql/SqliteResultValue.cc			sql/SqliteResultValue.hh
common_SOURCES += sql/SqliteProvider.cc				sql/SqliteProvider.hh
common_SOURCES += sql/Connection.cc
endif

if POSTGRES
if !SQLITE3
common_SOURCES += sql/Connection.cc
endif

common_CXXFLAGS += $(POSTGRESQL_CFLAGS)
common_LDFLAGS  += $(POSTGRESQL_LDFLAGS)
common_SOURCES += sql/PostgresArgListBuilder.cc sql/PostgresArgListBuilder.hh
common_SOURCES += sql/PostgresConnection.cc     sql/PostgresConnection.hh
common_SOURCES += sql/PostgresProvider.cc       sql/PostgresProvider.hh
endif

pkglib_LTLIBRARIES =
if WANT_LIBAPL
pkglib_LTLIBRARIES += libapl.la
pkginclude_HEADERS = libapl.h Error.def
libapl_la_SOURCES = $(common_SOURCES) libapl.h libapl.cc
libapl_la_LDFLAGS = $(common_LDFLAGS)
libapl_la_LDFLAGS += -avoid-version
# libapl_la_LDFLAGS += -module
# libapl_la_LDFLAGS += -shared
# libapl_la_LDFLAGS += -export-dynamic
libapl_la_CXXFLAGS = $(common_CXXFLAGS)
libapl_la_CXXFLAGS += $(GCOV_WANTED) $(GPROF_WANTED) $(CXX_RDYNAMIC)

else
if WANT_PYTHON

pkglib_LTLIBRARIES += lib_gnu_apl.la
lib_gnu_apl_la_SOURCES   = $(common_SOURCES) python_apl.cc
lib_gnu_apl_la_LDFLAGS   = $(common_LDFLAGS)
lib_gnu_apl_la_LDFLAGS  += -avoid-version
lib_gnu_apl_la_CXXFLAGS  = $(common_CXXFLAGS)
lib_gnu_apl_la_CXXFLAGS += $(GCOV_WANTED) $(GPROF_WANTED) $(CXX_RDYNAMIC)
lib_gnu_apl_la_CXXFLAGS += -DNDEBUG
lib_gnu_apl_la_CXXFLAGS += -D_FORTIFY_SOURCE=2
lib_gnu_apl_la_CXXFLAGS += -fwrapv
lib_gnu_apl_la_CXXFLAGS += -fPIC
lib_gnu_apl_la_CXXFLAGS += -fstack-protector
lib_gnu_apl_la_CXXFLAGS += --param=ssp-buffer-size=4
lib_gnu_apl_la_CXXFLAGS += -I/usr/include/python3.6m   # server68
lib_gnu_apl_la_CXXFLAGS += -I/usr/include/python3.8    # server58

else

bin_PROGRAMS = apl
apl_SOURCES = main.cc $(common_SOURCES)
apl_CXXFLAGS = $(common_CXXFLAGS)
apl_CXXFLAGS += $(GCOV_WANTED) $(GPROF_WANTED) $(CXX_RDYNAMIC)
apl_LDFLAGS = $(common_LDFLAGS)

endif
endif

# normally we would say:
#
# Svar_signals.hh: Svar_signals.def tcp_signal.m4
#
# but what if m4 is missing? So we explicitly 'make update-svar_signals'
# when Svar_signals.def or tcp_signal.m4 have changed (which rarely happens)
#
.PHONY:	update-svar_signals
update-svar_signals:
	m4 -D protocol=Svar_signals tcp_signal.m4 > Svar_signals.hh

apl_LDADD = $(LIBS)

apl.lines: apl
	objdump --section=.text --line-numbers --disassemble $< > $@

test: all
	./apl --id 1010 -T testcases/*tc

test1: all
	./apl --id 1010 --TM 3 -T testcases/*\.tc

test2: all
	./apl --id 1010 --TR -T testcases/*tc

# run testcases related to APs
testAP: all
	./apl --id 1010 -T testcases/AP*tc

perf: apl
	./apl --id 1010 --TM 3 -T testcases/Performance.pt


.PHONY:	patch
patch:
	mkdir -p patches
	svn diff > patches/`date +patch_%F-%R.diff`

.PHONY:	git-pull
git-pull:
	make -C emacs_mode $@
	make -C sql $@

.PHONY:	help
help:
	@echo ""
	@echo ""
	@echo "in addition to standard targets (all, install, clean, ...) "
	@echo "the following make targets may be supported (at this level):"
	@echo ""
	@echo "    make help                    - print this text"
	@echo "    make git-pull                - pull sql and emacs subdirs"
	@echo "    make update-svar_signals     - remake Svar_signals.hh"
	@echo ""

