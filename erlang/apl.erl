%% -*- coding: utf-8 -*-
%%
%%  This file is part of GNU APL, a free implementation of the
%%  ISO/IEC Standard 13751, "Programming Language APL, Extended"
%%
%%  Copyright (C) 2008-2017  Dr. Jürgen Sauermann
%%
%%  This program is free software: you can redistribute it and/or modify
%%  it under the terms of the GNU General Public License as published
%%  by the Free Software Foundation, either version 3 of the License, or
%%  (at your option) any later version.
%%
%%  This program is distributed in the hope that it will be useful,
%%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%%  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%%  GNU General Public License for more details.
%%
%%  You should have received a copy of the GNU General Public License
%%  along with this program.  If not, see <http://www.gnu.org/licenses/>.
%%

-module(apl).
-export([init/0, command/1, statement/1,
         eval_/1,
         eval_B/2, eval_AB/3,
         eval_LB/2, eval_ALB/3,
         eval_XB/3, eval_AXB/4,
         eval_LXB/3, eval_ALXB/4,
         eval_LRB/2, eval_ALRB/3,
         eval_LRXB/3, eval_ALRXB/4]).

% functions defined in erlang_APL_nif.c
%
-export([ command_utf8/1,     command_ucs/1,
          statement_utf8/1,   statement_ucs/1,
          fix_function_ucs/1, set_variable/3,
          eval_mux/5, e2c/1, e2a/1]).

init() -> erlang:load_nif("/usr/lib/apl/erlang_APL_nif", 0).

% dummy implementations for the functions defined in erlang_APL_nif.c...
%
command_utf8(_UTF8_string)       -> exit(command_utf8__not_loaded).
command_ucs(_)                   -> exit(command_ucs__not_loaded).
statement_utf8(_UTF8_string)     -> exit(statement_utf8__not_loaded).
statement_ucs(_)                 -> exit(statement_ucs__not_loaded).
fix_function_ucs(_UCS_string)    -> exit(fix_function_ucs__not_loaded).
set_variable(_UCS,_Shape,_Ravel) -> exit(set_variable___not_loaded).
eval_mux(_N, _A, _Fun, _X, _B)   -> exit(eval_mux__not_loaded).

% eval functions for different signatures

eval_     (   Fun0         ) -> eval_mux(-1, Fun0,    -1, 0,  0).
eval_B    (   Fun1,       B) -> eval_mux(-1, Fun1,    -1, B,  1).
eval_AB   (A, Fun2,       B) -> eval_mux( A, Fun2,    -1, B,  2).
eval_XB   (   Fun1,    X, B) -> eval_mux(-1, Fun1,     X, B,  3).
eval_AXB  (A, Fun2,    X, B) -> eval_mux( A, Fun2,     X, B,  4).
eval_LB   (   L_op1,      B) -> eval_mux(-1, L_op1,   -1, B,  5).
eval_ALB  (A, L_op1,      B) -> eval_mux( A, L_op1,   -1, B,  6).
eval_LXB  (   L_op1,   X, B) -> eval_mux(-1, L_op1,    X, B,  7).
eval_ALXB (A, L_op1,   X, B) -> eval_mux( A, L_op1,    X, B,  8).
eval_LRB  (   L_op2_R,    B) -> eval_mux(-1, L_op2_R, -1, B,  9).
eval_ALRB (A, L_op2_R,    B) -> eval_mux( A, L_op2_R, -1, B, 10).
eval_LRXB (   L_op2_R, X, B) -> eval_mux(0, L_op2_R, X, B, 11).
eval_ALRXB(A, L_op2_R, X, B) -> eval_mux(A, L_op2_R, X, B, 12).

% convert Erlang term X to a APL value cell
%
e2c(false)               -> 0;       % false == 0
e2c(true)                -> 1;       % true  == 1
e2c(X) when is_number(X) -> X;       % numeric scalar
e2c(X) when is_list(X) ->            % nested value
   {value, [length(X)], X};
e2c(X) -> X.

% convert Erlang term X to a APL value triple
%
%
e2a(false)               -> {value, [], [0]};            % false == 0
e2a(true)                -> {value, [], [1]};            % true  == 1
e2a(X) when is_number(X) -> {value, [], [X]};            % numeric scalar
e2a(X) when is_list(X) ->                                % vector
   Fun = fun(T, Acc) -> [e2c(T)|Acc] end,
   {value, [length(X)], lists:reverse(lists:foldl(Fun, [], X))};
e2a(X) -> {error, e2a, 'Invalid eterm', X}.

command(C)   -> command_ucs(C).
statement(S) -> statement_ucs(S).

