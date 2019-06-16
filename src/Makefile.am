EXTRA_DIRS =
if ! WANT_ANDROID
EXTRA_DIRS += APs emacs_mode sql workspaces
endif
SUBDIRS = native $(EXTRA_DIRS)
if HAVE_GTK3
SUBDIRS += Gtk
endif

DIST_SUBDIRS = $(SUBDIRS) testcases

datadir = @datadir@
localedir = $(datadir)/locale
host_os = @host_os@

distcleancheck_listfile = buildtag

# common means common to the apl and libapl builds
#
common_CXXFLAGS = -Wall -I sql $(CXX_WERROR)
common_LDFLAGS =
common_SOURCES =        buildtag		buildtag.hh		\
						../config.h		\
						APL_Float_as_class.hh	\
						APL_types.hh		\
Archive.cc					Archive.hh		\
						ArrayIterator.hh	\
Assert.cc					Assert.hh		\
Avec.cc			Avec.def		Avec.hh			\
Backtrace.cc					Backtrace.hh		\
Bif_F12_FORMAT.cc				Bif_F12_FORMAT.hh	\
Bif_F12_SORT.cc					Bif_F12_SORT.hh		\
Bif_F12_TAKE_DROP.cc				Bif_F12_TAKE_DROP.hh	\
Bif_OPER1_COMMUTE.cc				Bif_OPER1_COMMUTE.hh	\
Bif_OPER1_EACH.cc				Bif_OPER1_EACH.hh	\
Bif_OPER2_POWER.cc				Bif_OPER2_POWER.hh	\
Bif_OPER2_INNER.cc				Bif_OPER2_INNER.hh	\
Bif_OPER2_OUTER.cc				Bif_OPER2_OUTER.hh	\
Bif_OPER2_RANK.cc				Bif_OPER2_RANK.hh	\
Bif_OPER1_REDUCE.cc				Bif_OPER1_REDUCE.hh	\
Bif_OPER1_SCAN.cc				Bif_OPER1_SCAN.hh	\
CDR.cc						CDR.hh			\
Cell.cc			Cell.icc		Cell.hh			\
Doxy.cc						Doxy.hh			\
CharCell.cc					CharCell.hh		\
Command.cc		Command.def		Command.hh		\
Common.cc					Common.hh		\
ComplexCell.cc				    	ComplexCell.hh		\
configure_args.cc							\
DiffOut.cc					DiffOut.hh		\
DynamicObject.cc				DynamicObject.hh	\
Error.cc		Error.def		Error.hh		\
						Error_macros.hh		\
						ErrorCode.hh		\
Executable.cc					Executable.hh		\
FloatCell.cc					FloatCell.hh		\
Function.cc					Function.hh		\
DerivedFunction.cc				DerivedFunction.hh	\
			Help.def					\
						Heapsort.hh		\
Id.cc			Id.def			Id.hh			\
IndexExpr.cc					IndexExpr.hh		\
IndexIterator.cc				IndexIterator.hh	\
InputFile.cc					InputFile.hh		\
IntCell.cc					IntCell.hh		\
IO_Files.cc					IO_Files.hh		\
LApack.cc					LApack.hh		\
LibPaths.cc					LibPaths.hh		\
LineInput.cc					LineInput.hh		\
Logging.cc		Logging.def		Logging.hh		\
LvalCell.cc					LvalCell.hh		\
						makefile.h		\
Malloc_hooks.cc								\
Nabla.cc					Nabla.hh		\
Macro.cc		Macro.def		Macro.hh		\
NamedObject.cc					NamedObject.hh		\
NativeFunction.cc				NativeFunction.hh	\
						Native_interface.hh	\
NumericCell.cc					NumericCell.hh		\
Output.cc					Output.hh		\
						PJob.hh		\
Parser.cc					Parser.hh		\
Prefix.cc		Prefix.def		Prefix.hh		\
PointerCell.cc					PointerCell.hh		\
PrimitiveFunction.cc				PrimitiveFunction.hh	\
PrimitiveOperator.cc				PrimitiveOperator.hh	\
PrintBuffer.cc					PrintBuffer.hh		\
						PrintContext.hh		\
						PrintOperator.hh	\
QuadFunction.cc					QuadFunction.hh		\
ProcessorID.cc					ProcessorID.hh		\
Quad_CR.cc					Quad_CR.hh		\
Quad_DLX.cc					Quad_DLX.hh		\
Quad_FFT.cc					Quad_FFT.hh		\
Quad_FIO.cc					Quad_FIO.hh		\
Quad_FX.cc					Quad_FX.hh		\
Quad_GTK.cc					Quad_GTK.hh		\
Quad_PLOT.cc		Quad_PLOT.def		Quad_PLOT.hh		\
Quad_RE.cc					Quad_RE.hh		\
Quad_RL.cc					Quad_RL.hh		\
Quad_SQL.cc					Quad_SQL.hh		\
Quad_SVx.cc					Quad_SVx.hh		\
Quad_TF.cc					Quad_TF.hh		\
Quad_WA.cc					Quad_WA.hh		\
Parallel.cc					Parallel.hh		\
Performance.cc		Performance.def		Performance.hh		\
RealCell.cc					RealCell.hh		\
sbrk.cc									\
Shape.cc					Shape.hh		\
ScalarFunction.cc				ScalarFunction.hh	\
Security.cc		Security.def		Security.hh		\
StateIndicator.cc				StateIndicator.hh	\
Svar_DB.cc					Svar_DB.hh		\
Svar_record.cc					Svar_record.hh		\
tcp_signal.m4		Svar_signals.def	Svar_signals.hh		\
Symbol.cc					Symbol.hh		\
SymbolTable.cc					SymbolTable.hh		\
			SystemLimits.def	SystemLimits.hh		\
SystemVariable.cc	SystemVariable.def	SystemVariable.hh	\
TabExpansion.cc					TabExpansion.hh		\
			Thread_context.cc	Thread_context.hh	\
Token.cc		Token.def		Token.hh		\
						TokenEnums.hh		\
Tokenizer.cc					Tokenizer.hh		\
UCS_string.cc					UCS_string.hh		\
UCS_string_vector.cc				UCS_string_vector.hh    \
						Unicode.hh		\
UserFunction.cc					UserFunction.hh		\
UserFunction_header.cc				UserFunction_header.hh	\
UserPreferences.cc				UserPreferences.hh	\
UTF8_string.cc					UTF8_string.hh		\
Value.cc		Value.icc		Value.hh		\
			Value_P.icc		Value_P.hh		\
ValueHistory.cc					ValueHistory.hh		\
Workspace.cc					Workspace.hh            \
static_Objects.cc				static_Objects.hh

if HAVE_LIBPCRE2_32
common_SOURCES +=                                                       \
Regexp.cc                                       Regexp.hh
endif

if SQLITE3
common_CXXFLAGS += $(SQLITE3_CFLAGS)
common_LDFLAGS  += $(SQLITE3_LDFLAGS)
common_SOURCES += sql/SqliteArgListBuilder.cc sql/SqliteArgListBuilder.hh  \
                  sql/SqliteConnection.cc     sql/SqliteConnection.hh      \
                  sql/SqliteResultValue.cc    sql/SqliteResultValue.hh     \
                  sql/SqliteProvider.cc       sql/SqliteProvider.hh        \
                  sql/Connection.cc
endif

if POSTGRES
if !SQLITE3
common_SOURCES  += sql/Connection.cc
endif

common_CXXFLAGS += $(POSTGRESQL_CFLAGS)
common_LDFLAGS  += $(POSTGRESQL_LDFLAGS)
common_SOURCES += sql/PostgresArgListBuilder.cc sql/PostgresArgListBuilder.hh \
                  sql/PostgresConnection.cc     sql/PostgresConnection.hh \
                  sql/PostgresProvider.cc       sql/PostgresProvider.hh
endif

pkglib_LTLIBRARIES =
if WANT_LIBAPL
pkglib_LTLIBRARIES += libapl.la
pkginclude_HEADERS = libapl.h
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
lib_gnu_apl_la_CXXFLAGS += -I/usr/include/python3.4m

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
	objdump -j .text -l -d $< > $@

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


makefile.h:
	@echo "// some strings exported from Makefile"           > $@
	@echo "//"                                              >> $@
	@echo "#define Makefile__bindir     \"${bindir}\""      >> $@
	@echo "#define Makefile__docdir     \"${docdir}\""      >> $@
	@echo "#define Makefile__sysconfdir \"${sysconfdir}\""  >> $@
	@echo "#define Makefile__pkglibdir  \"${pkglibdir}\""   >> $@
	@echo "#define Makefile__localedir  \"${localedir}\""   >> $@
	@echo "#define Makefile__srcdir     \"${abs_srcdir}\""  >> $@
	@echo "#define Makefile__host_os    \"${host_os}\""     >> $@
	@echo                                                   >> $@

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

