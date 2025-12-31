
# progress: tell GNU APL that we have reached the start of this script
print("<→WRAPPER-STARTED→>.")

import sys
from sympy import *

# progress: tell GNU APL that we have imported all required modules
print("<→MODULES-IMPORTED→>.")

#----------------------------------------------------------------------------
umap = "⁰0¹1²2³3⁴4⁵5⁶6⁷7⁸8⁹9"
def map_upper(cc):
    """translate ¹ ² ³ ⁴ ⁵ ⁶ ⁷ ⁸ ⁹ to 0 1 2 3 4 5 6 7 8 9"""
    for j in range(len(umap)>>1):
        if cc == umap[2*j]: return umap[2*j + 1]
    print(f"Bad cc = {cc} in map_upper()")

def imap_upper(cc):
    """translate ¹ ² ³ ⁴ ⁵ ⁶ ⁷ ⁸ ⁹ to 0 1 2 3 4 5 6 7 8 9"""
    for j in range(len(umap)>>1):
        if cc == umap[2*j + 1]: return umap[2*j]
    print(f"Bad cc = {cc} in imap_upper()")

#----------------------------------------------------------------------------
power_mapped = False
def input_filter(str):
    ret = ""
    while len(str):
        (cc, str) = (str[0], str[1:])
        if "¹²³⁴⁵⁶⁷⁸⁹⁻".find(cc) != -1:
            ret += "**"
            ret += map_upper(cc)
            power_mapped = True
            continue;

        ret += cc
    return ret

#----------------------------------------------------------------------------
def output_filter_1(str):
    ret = ""
    while len(str):
        if str[0:2] == "**":
            str = str[2:]   # skip **
            if str[0] == "-":   ret += "⁻" ; str = str[1:]
            while "123456789".find(str[0]) != -1:
                (cc, str) = (str[0], str[1:])
                ret += imap_upper(cc)
                continue

        ret += str[0]
        str = str[1:]
    print(ret)

#----------------------------------------------------------------------------
def is_black(string):
    """return True unless string contains a non-whitespace"""
    for cc in string:
        if cc != " ":   return True
    return False
    
#----------------------------------------------------------------------------
class Matrix:
    def __init__(self, string):
        self.lines = string.split("\n")
    
    def __str__(self):
        ret = ""
        for row in self.lines:   ret += row + "\n"
        return ret
    
    def print(self):                        print(str(self))
    def row_numbers(self):                  return range(len(self.lines))
    def __getitem__(self, row):             return self.lines[row];
    def __setitem__(self, row, something):  self.lines[row] = something;
    def at(self, r, c):                     return self.lines[r][c]
    def set(self, r, c, something):
        self.lines[r] = self.lines[r][:c] + something + self.lines[r][c + 1:]
    def repeated_length(self, row, col):
        ret = 0
        while (col + ret < len(self.lines[row]) and
               self.at(row, col) == self.at(row, col + ret)):     ret += 1
        return ret

#----------------------------------------------------------------------------
def output_filter_2(str):
    """make the result of pretty() slighty more readable.

           4
       1. x   → x⁴
       2. remove empty lines
    """

    matrix = Matrix(str)
    for row in matrix.row_numbers():
        for col in range(len(matrix[row]) - 1):
            # power
            if (matrix.at(row, col) == "x"  and
               matrix.at(row - 1, col) == " "  and
               matrix.at(row, col + 1) == " " and
               "123456789".find(matrix.at(row - 1, col + 1)) != -1):
                   power = matrix.at(row - 1, col + 1)
                   matrix.set(row - 1, col + 1, " ")
                   matrix.set(row,     col + 1, imap_upper(power))

    for row in matrix.row_numbers():
        for col in range(len(matrix[row]) - 1):
            # root
            if (matrix.at(row, col) == "╱"      and
                matrix.at(row - 1, col) == " "  and
                matrix.at(row - 1, col + 1) == "_"):
                    len1 = matrix.repeated_length(row - 1, col + 1)
                    len2 = matrix.repeated_length(row,     col + 1)
                    if len1 <= len2:
                        for j in range(len1 + 1):
                            matrix.set(row - 1, col + j, " ");
                        for j in range(len1 + 1):

                            matrix.set(row,     col + j, "_");

    for row in matrix:
        if is_black(row):   print(row)

#----------------------------------------------------------------------------
program = sys.argv[0];
init_printing(use_unicode = True)

x = Symbol('x')

printer = input()
expr    = input_filter(input())
result = integrate(expr, x)

# See: https://docs.sympy.org/latest/tutorials/intro-tutorial/printing.html#tutorial-printing

if printer == "1":
    result = str(result)
    output_filter_1(result)
elif printer == "2":
    result = pretty(result)
    output_filter_2(result)
elif printer == "3":    # srep
    print(srepr(result))
elif printer == "4":    # LATEX
    print_latex(result)
elif printer == "5":    # mathml
    print_mathml(result)
elif printer == "6":    # Dot
    print(dotprint(result))

else:
    print(f"Bad printer: '{printer}'")

# progress: tell GNU APL that we have reached the end of this script
print(f"<→DONE→>. ({program})")

