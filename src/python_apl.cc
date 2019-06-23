
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Command.hh"
#include "ComplexCell.hh"
#include "DiffOut.hh"
#include "Error.hh"
#include "FloatCell.hh"
#include "LineInput.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Tokenizer.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

using namespace std;

static int display_mode = 1;

//-----------------------------------------------------------------------------
// initialization...

extern void init_1(const char * argv0, bool log_startup);
extern void init_2(bool log_startup);

void
do_init()
{
const bool log_startup = false;

//      uprefs.safe_mode = true;
   uprefs.user_do_svars = false;
   uprefs.system_do_svars = false;
   uprefs.requested_id = 3000;

   init_1("apl", log_startup);

   uprefs.read_config_file(true,  log_startup);   // in /etc/gnu-apl.d/
   uprefs.read_config_file(false, log_startup);   // in $HOME/.config/gnu_apl/

   init_2(log_startup);
}

inline void
init_if_necessary()
{
static bool init_done = false;
   if (!init_done) { do_init(); init_done = true; }
}
//-----------------------------------------------------------------------------
static PyObject *
apl_command(PyObject * self, PyObject * args)
{
const char * command = 0;
   if (!PyArg_ParseTuple(args, "s", &command))
       {
         CERR << "*** argument of command() is not a string." << endl;
         return 0;
       }

   init_if_necessary();

UTF8_string command_utf8(command);
UCS_string command_ucs(command_utf8);

ostringstream out;
  Command::do_APL_command(out, command_ucs);

  return PyUnicode_DecodeUTF8(out.str().data(), out.str().size(), 0);
}
//-----------------------------------------------------------------------------
static PyObject *
apl_to_python(const Value * value)
{
PyObject * shape = PyList_New(value->get_rank());
   loop(r, value->get_rank())
       PyList_SetItem(shape, r, PyLong_FromLong(value->get_shape_item(r)));

const ShapeItem ravel_len = value->nz_element_count();
PyObject * ravel = PyList_New(ravel_len);
   loop(i, ravel_len)
       {
         const Cell & cell = value->get_ravel(i);
         if (cell.is_integer_cell())
            PyList_SetItem(ravel, i, PyLong_FromLong(cell.get_int_value()));
         else if (cell.is_float_cell())
            PyList_SetItem(ravel, i, PyFloat_FromDouble(cell.get_real_value()));
         else if (cell.is_complex_cell())
            PyList_SetItem(ravel, i,
                           PyComplex_FromDoubles(cell.get_real_value(),
                                                 cell.get_imag_value()));
         else if (cell.is_character_cell())
            {
              const UCS_string ucs(cell.get_char_value());   // 1-element string
              UTF8_string utf(ucs);
              PyList_SetItem(ravel, i, PyUnicode_FromStringAndSize(utf.c_str(),
                                                                   utf.size()));
            }
         else if (cell.is_pointer_cell())
            {
              Value_P sub_val = cell.get_pointer_value();
              PyList_SetItem(ravel, i, apl_to_python(sub_val.get()));
            }
         else
            {
               CERR << "*** Bad cell type at " LOC << endl;
               return 0;
            }
       }

   return PyTuple_Pack(2, shape, ravel);
}
//-----------------------------------------------------------------------------
static PyObject * exec_result = 0;

bool
python_result_callback(Token & result)
{
const TokenTag tag = result.get_tag();
bool do_display = false;

   if (tag == TOK_APL_VALUE1 || tag == TOK_APL_VALUE3)   // non-committed value
      {
        do_display = display_mode > 0;
        exec_result = PyTuple_Pack(2, PyLong_FromLong(1),
                                   apl_to_python(result.get_apl_val().get()));

      }
   else if (tag == TOK_APL_VALUE2)                       // committed value
      {
        do_display = display_mode > 1;
        exec_result = PyTuple_Pack(2, PyLong_FromLong(2),
                                   apl_to_python(result.get_apl_val().get()));
      }
   else if (tag == TOK_VOID)                            // void
      {
        exec_result = PyTuple_Pack(2, PyLong_FromLong(3), Py_None);
      }
   else if (tag == TOK_BRANCH)                           // →N
      {
        exec_result = PyTuple_Pack(2, PyLong_FromLong(4),
                                   PyLong_FromLong(result.get_int_val()));
      }
   else if (tag == TOK_NOBRANCH)                         // →''
      {
        // never called because TOK_NOBRANCH is a NOOP
        exec_result = PyTuple_Pack(2, PyLong_FromLong(4), Py_None);
      }
   else if (tag == TOK_ESCAPE)                           // →
      {
        exec_result = PyTuple_Pack(2, PyLong_FromLong(5), Py_None);
      }
   else
      {
        CERR << "Unexpected Tag << HEX(tag) at " LOC << endl;
        FIXME;
      }

   return do_display;
}
//-----------------------------------------------------------------------------
static PyObject *
apl_exec(PyObject * self, PyObject * args)
{
const char * line = 0;
    if (!PyArg_ParseTuple(args, "s", &line))
       {
         CERR << "*** argument of exec() is not a string." << endl;
         return 0;
       }

   init_if_necessary();

UTF8_string line_utf8(line);
UCS_string line_ucs(line_utf8);

   exec_result = 0;
   try { Command::process_line(line_ucs); }
   catch (const Error & error)
      {
        CERR << "cought Error" << endl;
        return PyLong_FromLong(error.get_error_code());
      }
   catch (ErrorCode error_code)
      {
        CERR << "cought ErrorCode" << endl;
        return PyLong_FromLong(error_code);
      }
   catch (...)
      {
        CERR << "cought something else" << endl;
        return PyLong_FromLong(-1);
      }

   if (exec_result == 0)   // error or defined function without return value
      {
        if (const StateIndicator * si = Workspace::SI_top_error())
           {
             const Error & err = StateIndicator::get_error(si);
             const ErrorCode ec = err.get_error_code();
             return PyTuple_Pack(2, PyLong_FromLong(0), PyLong_FromLong(ec));
           }

        // defined function without result
        //
        return PyTuple_Pack(2, PyLong_FromLong(3), Py_None);
      }

PyObject * ret = exec_result;
   exec_result = 0;
   return ret;
}
//-----------------------------------------------------------------------------
static const Value *
apl_get_var_value(PyObject * args)
{
const char * varname = 0;
   if (!PyArg_ParseTuple(args, "s", &varname))
      {
        CERR << "*** argument of get_shape(varname) is not a string." << endl;
        return 0;
      }

   init_if_necessary();

UTF8_string varname_utf8(varname);
UCS_string varname_ucs(varname_utf8);
Symbol * sym = Workspace::lookup_existing_symbol(varname_ucs);
   if (sym == 0)
      {
        CERR << "*** " << varname_ucs << "is not an APL symbol." << endl;
         return 0;
      }

const ValueStackItem * top = sym->top_of_stack();
   if (top == 0 || top->name_class != NC_VARIABLE)
      {
        CERR << "*** " << varname_ucs << "is not an APL variable." << endl;
        return 0;
      }

   return top->apl_val.get();
}
//-----------------------------------------------------------------------------
static PyObject *
make_shape(const Shape & shape)
{
PyObject * result = PyList_New(shape.get_rank());
   loop(axis, shape.get_rank())
      PyList_SetItem(result, axis, PyLong_FromLong(shape.get_shape_item(axis)));

   return result;
}
//-----------------------------------------------------------------------------
static PyObject *
apl_get_shape(PyObject * self, PyObject * args)
{
const Value * value = apl_get_var_value(args);
   if (value == 0)   return 0;

   return make_shape(value->get_shape());
}
//-----------------------------------------------------------------------------
static PyObject *
make_ravel(const Value * value)
{
const ShapeItem len = value->nz_element_count();
PyObject * result = PyList_New(len);

   loop(l, len)
      {
        const Cell & cell = value->get_ravel(l);
        PyObject * item = Py_None;
        if (cell.is_integer_cell())
           {
             item = PyLong_FromLong(cell.get_int_value());
           }
        else if (cell.is_float_cell())
           {
             item = PyFloat_FromDouble(cell.get_real_value());
           }
        else
           {
             CERR << "*** Warning: unsupported Cell type" << endl;
           }
        PyList_SetItem(result, l, item);
      }

   return result;
}
//-----------------------------------------------------------------------------
static PyObject *
apl_get_ravel(PyObject * self, PyObject * args)
{
const Value * value = apl_get_var_value(args);
   if (value == 0)   return 0;
   return make_ravel(value);
}
//-----------------------------------------------------------------------------
static PyObject *
apl_get_value(PyObject * self, PyObject * args)
{
const Value * value = apl_get_var_value(args);
   if (value == 0)   return 0;

PyObject * shape = make_shape(value->get_shape());
PyObject * ravel = make_ravel(value);
   return PyTuple_Pack(2, shape, ravel);
}
//-----------------------------------------------------------------------------
static Shape
list_to_shape(PyObject * shape)
{
Shape ret;
   if (!PyList_Check(shape))
      {
        CERR << "*** tuple[1] is not a list at " LOC << endl;
        return ret;
      }

const Rank rank = PyList_Size(shape);
   loop(r, rank)
       {
         PyObject * axis = PyList_GetItem(shape, r);
          ret.add_shape_item(PyLong_AsLong(axis));
       }
   return ret;
}
//-----------------------------------------------------------------------------
Shape
shape_for_item(PyObject * item)
{
   // compute the shape of a given item. The shape is:
   //
   // [ len(list) ]     for python lists, and
   // shape             for tuples (ravel, shape)
   // []                otherwise (primitive python types)
   //
   if (PyList_Check(item))     return Shape(PyList_Size(item));
   if (!PyTuple_Check(item))   return Shape();
   if (PyTuple_Size(item) != 2)
      {
        CERR << "*** tuple len ≠ 2 at " LOC << endl;
        return Shape();
      }

PyObject * shape = PyTuple_GetItem(item, 1);
   return list_to_shape(shape);
}
//-----------------------------------------------------------------------------
static Value_P python_to_apl(PyObject * ravel, PyObject * shape);

static Value_P
python_to_apl(PyObject * ravel, const Shape & shape)
{
Value_P Z(shape, LOC);
const ShapeItem len_Z = Z->nz_element_count();

   if (PyComplex_Check(ravel))   // ravel is a complex scalar
      {
        const double real = PyComplex_RealAsDouble(ravel);
        const double imag = PyComplex_ImagAsDouble(ravel);
        loop(z, len_Z)   new (Z->next_ravel())   ComplexCell(real, imag);
      }
   else if (PyFloat_Check(ravel))   // ravel is a floating point scalar
      {
        const double real = PyFloat_AsDouble(ravel);
        loop(z, len_Z)   new (Z->next_ravel())   FloatCell(real);
      }
   else if (PyLong_Check(ravel))   // ravel is an integer scalar
      {
        const APL_Integer ival = PyLong_AsLong(ravel);
        loop(z, len_Z)   new (Z->next_ravel())   IntCell(ival);
      }
   else if (PyUnicode_Check(ravel))   // ravel is a char scalar or a string
      {
        Py_ssize_t len = 0;
        const UTF8 * data = reinterpret_cast<const UTF8 * >
                                (PyUnicode_AsUTF8AndSize(ravel, &len));
        UTF8_string utf(data, len);
        UCS_string ucs(utf);
        loop(z, len_Z)   new (Z->next_ravel())   CharCell(ucs[z % ucs.size()]);
      }
   else if (PyTuple_Check(ravel))   // item is a tuple (val, shape)
      {
        PyObject * sub_shape = PyTuple_GetItem(ravel, 1);
        PyObject * sub_ravel = PyTuple_GetItem(ravel, 0);
        Value_P sub = python_to_apl(sub_ravel, sub_shape);
        loop(z, len_Z)
            new (Z->next_ravel()) PointerCell(sub.get(), Z.getref());
      }
   else if (PyList_Check(ravel))   // ravel is a list
      {
        const ShapeItem src_len = PyList_Size(ravel);
        loop(z, len_Z)
            {
              PyObject * src = PyList_GetItem(ravel, z % src_len);
              Value_P sub = python_to_apl(src, 0);
              Z->next_ravel()->init_from_value(sub.get(), Z.getref(), LOC);
            }
      }
   else
      {
        CERR << "*** " << "ravel is something else" << endl;
        FIXME;
      }

   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
static Value_P
python_to_apl(PyObject * ravel, PyObject * shape)
{
   if (shape)   return python_to_apl(ravel, list_to_shape(shape));
   else         return python_to_apl(ravel, shape_for_item(ravel));
}
//-----------------------------------------------------------------------------
static PyObject *
apl_set_value(PyObject * self, PyObject * args)
{
const char * varname = 0;
PyObject * ravel = 0;
PyObject * shape = 0;   // optional

   if (!PyTuple_Check(args))   return 0;   // never

const int arg_count = PyTuple_Size(args);
   if (arg_count == 2)   // set_value(varname, ravel)
      {
        if (!PyArg_ParseTuple(args, "sO", &varname, &ravel))
           {
              CERR << "*** Bad arguments in set_value(varname, ravel) ."
                   << endl;
              return 0;
           }
      }
   else if (arg_count == 3)   // set_value(varname, ravel, shape)
      {
        if (!PyArg_ParseTuple(args, "sOO", &varname, &ravel, shape))
           {
              CERR << "*** Bad arguments in set_value(varname, ravel, shape) ."
                   << endl;
              return 0;
           }
      }
   else if (arg_count > 3)
      {
        CERR << "*** Too many (" << arg_count << ") arguments in "
                "set_value(varname, ravel, [shape]) ." << endl;
        return 0;
      }
   else
      {
        CERR << "*** Too few (" << arg_count << ") arguments in "
                "set_value(varname, ravel, [shape]) ." << endl;
        return 0;
      }

   init_if_necessary();

UTF8_string varname_utf8(varname);
UCS_string varname_ucs(varname_utf8);
Symbol * sym = Workspace::lookup_symbol(varname_ucs);
   if (sym == 0)
      {
        CERR << "*** " << varname_ucs << "cannot be assigned." << endl;
         return 0;
      }

Value_P value = python_to_apl(ravel, shape);
   if (!value)
      {
        CERR << "*** " << ravel << "could not be assigned." << endl;
         return 0;
      }

   sym->assign(value, true, LOC);
   return Py_None;
}
//-----------------------------------------------------------------------------
static PyObject *
apl_fix_function(PyObject * self, PyObject * args)
{
const char * text = 0;
    if (!PyArg_ParseTuple(args, "s", &text))
       {
         CERR << "*** argument of fix_function() is not a string." << endl;
         return 0;
       }

   init_if_necessary();

const UTF8_string text_utf8(text);
const UCS_string text_ucs(text_utf8);
int error_line = 0;

const UTF8_string creator("python");
UserFunction * fun = UserFunction::fix(text_ucs, error_line, false, LOC,
                                       creator, true);

   if (fun)   return Py_None;

   CERR << "*** invalid string argument of fix_function()" << endl
        << "*** offending function line: " << error_line << endl;

   return PyLong_FromLong(error_line);
}
//-----------------------------------------------------------------------------
static PyObject *
apl_set_display(PyObject * self, PyObject * args)
{
int mode;
    if (!PyArg_ParseTuple(args, "i", &mode))
       {
         CERR << "*** argument of apl_set_display() is not an integer." << endl;
         return 0;
       }

   if (mode < 0 || mode > 2)
       {
         CERR << "*** invalid mode argument (" << mode
              << ") for apl_set_display()." << endl;
         return 0;
       }

const int oldmode = display_mode;
   display_mode = mode;
   return PyLong_FromLong(oldmode);
}
//=============================================================================
const char * DESCR_help =
"gnu_apl.help() : print help for a topic.\n"
"\n"
"Synopsis: gnu_apl.help(topic)\n"
"   topic is an UTF8 string containing a help topic.\n"
"\n"
"Examples:\n"
"    gnu_apl.help()           (this text)\n"
"    gnu_apl.help('all')      (long)\n"
"    gnu_apl.help('help')\n"
"    gnu_apl.help('command')\n"
"    gnu_apl.help('exec')\n"
"    gnu_apl.help('fix_function')\n"
"    gnu_apl.help('get_ravel')\n"
"    gnu_apl.help('get_shape')\n"
"    gnu_apl.help('get_value')\n"
"    gnu_apl.help('set_value')\n"
"    gnu_apl.help('APL-values')\n"
"\n"
;

const char * DESCR_command =
"gnu_apl.command() : execute an APL command.\n"
"\n"
"Synopsis: Result = gnu_apl.command(cmd)\n"
"   cmd is an UTF8 string containing a valid APL expression\n"
"\n"
"Result:\n"
"   a UTF8 string that contains the output of the command (which depends on\n"
"   the command that was executed).\n"
;

const char * DESCR_exec =
"gnu_apl.exec() : execute an APL expression.\n"
"\n"
"Synopsis: Result = gnu_apl.exec(expr)\n"
"   expr is an UTF8 string containing a valid APL expression\n"
"\n"
"Result:\n"
"    tuple(0, error_code)   if the expression caused an error\n"
"    tuple(1, value)        if the expression returned an uncommitted value\n"
"    tuple(2, value)        if the expression returned a committed value\n"
"    tuple(3, None)         if the expression returned no value\n"
"    tuple(4, value)        if the expression returned a branch (e.g. →N)\n"
"    tuple(5, value)        if the expression returned branch escape (→)\n"
"\n"
"Note: Result has the same format as ⎕EC (execute controlled)\n"
;

const char * DESCR_fix_function =
"gnu_apl.fix_function() : create (aka. ⎕FX) a new defined APL function, or\n"
"                         change the definition of an existing definition.\n"
"\n"
"Synopsis: Result = gnu_apl.fix_function(text)\n"
"    text is a UTF8 string containing the function lines (separated by \\n).\n"
"    The first line in text is the function signature, e.g. Z←A myfun B\n"
"    The subsequent lines in text comprise the function body.\n"
"\n"
"Result:\n"
"    None       if the function could be created,\n"
"    int(0)     if the function signature was incorrect, \n"
"    int(N)     if the body line N ≥ 1 was incorrect.\n"
"    int(-1)    if something else went wrong.\n"
"\n"
"NOTE: creating or modifying defined APL functions with ⎕FX (in APL) or by\n"
"    gnu_apl.fix_function() (in Python) is not the usual way of creating or\n"
"    modifying defined APL functions. Most APL programmers will prefer to use\n"
"    the standard APL ∇-editor (in interactive APL sessions) or to write APL\n"
"    scripts in order to create an APL workspace that contains the desired\n"
"    defined functions. The workspace is then loaded with APL commands\n"
"    )LOAD or )COPY (in APL) resp. gnu_apl.command(')LOAD ...') or\n"
"    gnu_apl.command(')COPY ...') in Python."
;

const char * DESCR_get_ravel =
"gnu_apl.get_ravel() : get the ravel of an APL variable.\n"
"\n"
"Synopsis: Result = gnu_apl.get_ravel(text)\n"
"    varname is a string containing the name of an APL variable.\n"
"\n"
"Result:\n"
"   on sucess:   list [ i₁, i₂, ... iN ] where N is the length of the\n"
"                ravel (aka, ,varname in APL) of the value of the variable,\n"
"                and iK is the K'th ravel element:\n"
"                    int(iK):       an APL integer\n"
"                    float(iK):     an APL float\n"
"                    complex(iK):   an APL complex value\n"
"                    chr(iK):       an APL character (Unicode)\n"
"                    list(iK):      a nested APL sub-value\n"
"                    tuple(R, S):   a nested APL sub-value with ravel R\n"
"                                   and shape S\n"
"   otherwise:   int(error_code)\n"
"\n"
"Example:\n"
"    gnu_apl.exec('Var←4 4⍴⍳16')\n"
"(2, ([4, 4], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]))\n"
"    gnu_apl.get_ravel('Var')\n"
"[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]\n"
;

const char * DESCR_get_shape =
"gnu_apl.get_shape() : get the shape of an APL variable.\n"
"\n"
"Synopsis: Result = gnu_apl.get_shape(text)\n"
"    varname is a string containing the name of an APL variable.\n"
"\n"
"Result:\n"
"    on sucess:   list [ r₁, r₂, ... rR ] where R is the number of axes\n"
"                 (aka. the rank in APL) of the value of the variable and\n"
"                 rK is the length of the Kth axis.\n"
"    otherwise:   int(error_code)\n"
"\n"
"Example:\n"
"    gnu_apl.exec('Var←4 4⍴⍳16')\n"
"(2, ([4, 4], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]))\n"
"    gnu_apl.get_shape('Var')\n"
"[4, 4]\n"
;

const char * DESCR_get_value =
"gnu_apl.get_value() : get the value of an APL variable.\n"
"\n"
"Synopsis: Result = gnu_apl.get_value(varname)\n"
"    varname is a string containing the name of an APL variable.\n"
"\n"
"Result:\n"
"    on sucess:   tuple( get_ravel(varname), get_shape(varname) )\n"
"    otherwise:   int(error_code)\n"
"\n"
"Example:\n"
"    gnu_apl.exec('Var←4 4⍴⍳16')\n"
"(2, ([4, 4], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]))\n"
"    gnu_apl.get_value('Var')\n"
"([4, 4], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])\n"
;

const char * DESCR_set_display =
"gnu_apl.set_display() : set the APL display mode.\n"
"\n"
"Synopsis: Result = gnu_apl.set_display(mode)\n"
"    mode is an integer:\n"
"        0:     display neither committed values nor not committed values\n"
"        1:     display only not committed values (standard APL behavior)\n"
"        2:     display both committed values and not committed values\n"
"\n"
"Result: the previous display mode\n"
;

const char * DESCR_values =
"APL-values : The mapping between Python values and APL values (examples):\n"
"\n"
"╔═══════════════════════════╤══╤════════════╤═══════════════════════════════╗\n"
"║ Python value              │  │ APL value  │ Remark                        ║\n"
"╠═══════════════════════════╪══╪════════════╪════════════╤══════════════════╣\n"
"║ int(65)                   │ →│ 65         │            │ Integer          ║\n"
"╟───────────────────────────┼──┼────────────┤            ├──────────────────╢\n"
"║ float(6.5)                │ →│ 6.5        │            │ Floating point   ║\n"
"╟───────────────────────────┼──┼────────────┤ APL Scalar ├──────────────────╢\n"
"║ complex(6, 5)             │ →│ 6J5        │            │ Complex          ║\n"
"╟───────────────────────────┼──┼────────────┤            ├──────────────────╢\n"
"║ chr(65) or 'A'            │ →│ 'A'        │            │ single Unicode   ║\n"
"╠═══════════════════════════╪══╪════════════╪════════════╪══════════════════╣\n"
"║ list([6, 5, 'A'])         │ →│ 6 5 'A'    │            │ Mixed            ║\n"
"╟───────────────────────────┼──┼────────────┤ APL Vector ├──────────────────╢\n"
"║ str('string')             │ →│ 'string'   │            │ Character        ║\n"
"╠═══════════════════════════╪══╪════════════╪════════════╪══════════════════╣\n"
"║ tuple([65],  [])          │←→│ 65         │ APL Scalar │ Integer          ║\n"
"║ tuple([6.5], [])          │←→│ 6.5        │ (as above) │ Floating point   ║\n"
"║ tuple([6J5], [])          │←→│ 6J5        │ in tuple() │ Complex          ║\n"
"║ tuple(['A'], [])          │←→│ 'A'        │ format     │ single Unicode   ║\n"
"╟───────────────────────────┼──┼────────────┼────────────┴──────────────────╢\n"
"║ tuple([6, 5, 'A'], [3])   │←→│ 6 5 'A'    │ Vector of length 3 (as above) ║\n"
"╟───────────────────────────┼──┼────────────┼───────────────────────────────╢\n"
"║ ravel = [6, 5, 'A']       │  │ 6 5 'A'    │ 2×3 Matrix. The ravel is      ║\n"
"║ shape = [2, 3]            │←→│ 6 5 'A'    │ truncated or repeated as      ║\n"
"║ tuple(ravel, shape)       │  │            │ needed to fill the shape      ║\n"
"╟───────────────────────────┼──┼────────────┼───────────────────────────────╢\n"
"║ any non-scalar array item │  │ nested APL │ 3-element vector with nested  ║\n"
"║ (such as str() or list()  │  │ array      │ second element which is the   ║\n"
"║ or tuple() above)         │  │            │ 2-element sub-vector 2 3      ║\n"
"║ [1 [2, 3] 4]              │←→│ 1 (2 3) 4  │                               ║\n"
"╚═══════════════════════════╧══╧════════════╧═══════════════════════════════╝\n"
"\n"
"NOTE: Every non-tuple() variant (like str('string') or list([i₁, i₂, ...])\n"
"      above is a convenience shortcut that can also be expressed in the form\n"
"      of the more general tuple() variant. For performance reasons, but also\n"
"      to simplify the decoding of APL values in Python, these convenience\n"
"      shortcuts work only in the Python → APL direction; values in the\n"
"      APL → Python direction are always returned in the tuple() format.\n"
;

const char * DESCR_set_value =
"gnu_apl.set_value() : set the value of an APL variable.\n"
"                      The variable is created if necessary.\n"
"\n"
"Synopsis: gnu_apl.set_value(varname, value)\n"
"    varname is a UTF8 string containing the name of an APL variable.\n"
"    value is the new value of the variable, see gnu_apl.help('APL-values').\n"
"\n"
"Examples:\n"
"    gnu_apl.set_value('Var', 65)                # Var ← 65\n"
"    gnu_apl.set_value('Var', 6.5)               # Var ← 6.5\n"
"    gnu_apl.set_value('Var', 6J5)               # Var ← 6J5\n"
"    gnu_apl.set_value('Var', [6, 5, 'A'])       # Var ← 6 5 'A'\n"
"    gnu_apl.set_value('Var', [1, [2, 3], 4])    # Var ← 1 (2 3) 4\n"
;

//=============================================================================
static PyObject *
apl_help(PyObject * self, PyObject * args)
{
const char * topic = 0;
   if (PyArg_ParseTuple(args, "s", &topic))   // got optional topic
       {
         const char * help = 0;
         if      (!strcmp(topic, "help"))           help = DESCR_help;
         else if (!strcmp(topic, "command"))        help = DESCR_command;
         else if (!strcmp(topic, "exec"))           help = DESCR_exec;
         else if (!strcmp(topic, "fix_function"))   help = DESCR_fix_function;
         else if (!strcmp(topic, "get_ravel"))      help = DESCR_get_ravel;
         else if (!strcmp(topic, "get_shape"))      help = DESCR_get_shape;
         else if (!strcmp(topic, "get_value"))      help = DESCR_get_value;
         else if (!strcmp(topic, "set_value"))      help = DESCR_set_value;
         else if (!strcmp(topic, "set_display"))    help = DESCR_set_display;
         else if (!strcmp(topic, "APL-values"))     help = DESCR_values;
         else if (!strcmp(topic, "all"))
            {
              const char * sep = "----------------------------------------"
                                 "---------------------------------------\n";
              CERR << DESCR_help         << sep
                   << DESCR_command      << sep
                   << DESCR_exec         << sep
                   << DESCR_fix_function << sep
                   << DESCR_get_ravel    << sep
                   << DESCR_get_shape    << sep
                   << DESCR_get_value    << sep
                   << DESCR_set_value    << sep
                   << DESCR_set_display  << sep
                   << DESCR_values       << sep << endl;
              return Py_None;
            }

         if (help == 0)
            {
              CERR << "*** " << topic << " is not a help() topic" << endl;
            }
         else
            {
              CERR << help << endl;
            }
       }
   else                                       // no topic
       {
         CERR << DESCR_help << endl;
       }

   return Py_None;
}
//-----------------------------------------------------------------------------
static PyMethodDef AplMethods[] =
{
  { "help",         apl_help,         METH_VARARGS, DESCR_help         },
  { "command",      apl_command,      METH_VARARGS, DESCR_command      },
  { "exec",         apl_exec,         METH_VARARGS, DESCR_exec         },
  { "fix_function", apl_fix_function, METH_VARARGS, DESCR_fix_function },
  { "get_ravel",    apl_get_ravel,    METH_VARARGS, DESCR_get_ravel    },
  { "get_shape",    apl_get_shape,    METH_VARARGS, DESCR_get_shape    },
  { "get_value",    apl_get_value,    METH_VARARGS, DESCR_get_value    },
  { "set_value",    apl_set_value,    METH_VARARGS, DESCR_set_value    },
  { "set_display",  apl_set_display,  METH_VARARGS, DESCR_set_display  },
  { NULL,           0,                0,            0 }   /* Sentinel */
};
//-----------------------------------------------------------------------------
static struct PyModuleDef apl_module =
{
  PyModuleDef_HEAD_INIT,
  "gnu_apl",   /* name of module */
  0,           // spam_doc, /* module documentation, may be NULL */
  -1,          /* size of per-interpreter state of the module,
                  -1 if the module keeps state in global variables. */
  AplMethods
};
//-----------------------------------------------------------------------------
PyMODINIT_FUNC
PyInit_gnu_apl(void)
{
    return PyModule_Create(&apl_module);
}
//=============================================================================
