#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ finance.apl Workspace for calculations involving interest and time.
⍝ Copyright (C) 2020 Bill Daly

⍝ This program is free software: you can redistribute it and/or modify
⍝ it under the terms of the GNU General Public License as published by
⍝ the Free Software Foundation, either version 3 of the License, or
⍝ (at your option) any later version.

⍝ This program is distributed in the hope that it will be useful,
⍝ but WITHOUT ANY WARRANTY; without even the implied warranty of
⍝ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
⍝ GNU General Public License for more details.

⍝ You should have received a copy of the GNU General Public License
⍝ along with this program.  If not, see <http://www.gnu.org/licenses/>.

⍝ ********************************************************************

)copy_once 5 DALY/utl
)copy_once 3 DALY/lex
)copy_once 3 DALY/date

⍝ ********************************************************************
⍝			   Basic Functions
⍝ ********************************************************************

∇pymt←fin∆annuityPayment args;pv;i;n
 ⍝ Function returns the payment to amortize a single ammount. IE
 ⍝ Amount of a mortgage payment. Argument is the single amount,
 ⍝ interest per period, and number of periods.
 args←3↑,args ◊ pv←args[1] ◊ i←args[2] ◊ n←args[3]
 pymt←pv×i÷1 - (1 + i)*-n
∇

∇value←fin∆compoundAnnuity args;payment;i;n
 ⍝ Function calculates the future value of annuity.  That is the
 ⍝ amount in a savings account after n periods of depositing the same
 ⍝ amount. Arguments are payment, interest per period and number of
 ⍝ periods.
 payment←args[1] ◊ i ← args[2] ◊ n← args[3]
 value←+/payment × (1+i) * ⍳n
∇

∇value←fin∆compoundValue args;cash;i;n
 ⍝ Function calculates the future value of a single sum.  Arguments
 ⍝ are amount of cash invested, interest rate per periood, and number
 ⍝ of periods.
 args←3↑,args
 cash←args[1] ◊ i ← args[2] ◊ n←args[3]
 value ← cash × (1 + i)*n
∇

∇value←fin∆netPresentValue args;i;flow
 ⍝ Function calculates the net present value of a series of a cash
 ⍝ flow vector. Arguments the interest rate and a vector of cash
 ⍝ payments.
 i←1⊃args
 flow←1↓args
 value←+/flow ÷ (1+i) * ⍳⍴flow←,flow
∇

∇value←fin∆presentValue args;cash;i;n
 ⍝ Function calculates the present value of a single sum payable in n
 ⍝ periods. Argument are future cash, interest rate per period, and
 ⍝ number of periods.
 args←3↑,args
 cash←args[1] ◊ i ← args[2] ◊ n←args[3]
 value←cash ÷ (1 + i) * n
∇

∇loan ← fin∆presentValueAnnuity args;payment;i;n
 ⍝ Function calculates the present value of an annuity. That is the
 ⍝ amount of a loan today in exchange for a payment of args[1] each
 ⍝ period for n periods at i interest per period.  Arguments are
 ⍝ payment, interest and number of periods.
 payment←args[1] ◊ i←args[2] ◊ n←args[3]
 loan ← +/payment÷(1+i)*⍳n
∇

∇rate← fin∆irr args;guess;flow;old_rate;new_rate;rate;old_pv;pv
 ⍝ Function calculates the internal rate of return of a cash flow
 ⍝ vector. The right argument is a vector of estimated interest rate
 ⍝ and the cash flow.
 guess←1⊃args
 flow←1↓args
 old_rate←0 ◊ old_pv← +/flow
 rate←guess
 st:
 pv← fin∆netPresentValue rate, flow
 new_rate←rate-pv×(-/rate-old_rate)÷pv-old_pv
 old_rate←rate
 rate←new_rate
 old_pv←pv
 →(1<|pv)/st
∇

⍝ ********************************************************************
⍝			  Dateflow Functions
⍝ ********************************************************************

∇b←fin∆dfp df
 ⍝ Function to test that df is a dateflow.
 →(~b←⊃2=⍴⍴df)/0
 →(~b←⊃2=1↑⍴df)/0
 →(~b←⊃∧/utl∆integerp ¨ df[1;])/0
 b←⊃∧/utl∆numberp df[2;]
∇

∇sum← df1 fin∆df∆add df2;b1;b2;t3;expanded_df1
 ⍝ Function to add two dateflows
 ⍎(1=⍴⍴df2)/'df2←2 1⍴df2'
 ⍎(1=⍴⍴df1)/'df1←2 1⍴df1'
 b1←(df1[1;]∊df2[1;])
 b2←(df2[1;]∊df1[1;])
 t3←b1\b2/df2
 t3[1;(~b1)/⍳⍴b1]←(~b1)/df1[1;]
 t3←t3,(~b2)/df2
 sum←t3[;⍋t3[1;]]
 expanded_df1←df1,(~b2)/df2[1;],[.1]0
 expanded_df1←expanded_df1[;⍋expanded_df1[1;]]
 sum[2;]←sum[2;]+expanded_df1[2;]
∇

∇table←fin∆df∆amortizationTable args;df;i;year;lb;ix;bal;bv;peryear
 ⍝ Function returns a table showing the implied amortization of a
 ⍝ dateflow item. Args is: the Dateflow; interest rate to use.
 df←1⊃args
 i←(2⊃args)÷365
 ⍝ year←1+(date∆unlillian df[1;1])[1]
 ⍝ peryear←+/year ={(date∆unlillian ⍵)[1]}¨df[1;]
 ⍝ ⍎(peryear=0)/'peryear←1↓⍴df'
 ⍝ i←i÷peryear
 table←⍉df
 table[;2]←-table[;2]
 table←1 0 1 0\table
 lb←((1↑⍴table)⍴st),ed
 bal←0
 ix←2
 table[1;4]←table[1;3]
 st:
 table[ix;4]←¯2 utl∆round table[ix;3]+table[ix-1;4]×(1+i)*-/table[ix+0 ¯1;1]
 table[ix;2]←table[ix;4]-+table[ix;3]+table[ix-1;4]
 →lb[ix←ix+1]
 ed:
 table[;1]←{'0006/06/06'⍕date∆unlillian ⍵}¨table[;1]
∇

∇a←fin∆continuouslyCompounded args;p;i;term
 ⍝ Function returns a principal sum continously compounded. Args is:
 ⍝ pricipal interestRate term
 p←1⊃args
 i←2⊃args
 term←3⊃args
 a←p×*i×term
∇

∇df←fin∆df∆init col1
 ⍝ Function creates a new datefow from the data for the first column.
 df←2 1⍴,col1
∇

∇rate←fin∆df∆irr args;guess;df;new_rate;old_rate;old_pv;pv
 ⍝ Function returns the internal rate of return of a dateflow. Args
 ⍝ are a dateflow and a guess of the interest rate.
 df←1⊃args
 guess←2⊃args
 utl∆es (~(∨/df[2;]<0)∧∨/df[2;]>0)/'A dateflow array must have both positive and negative amounts.'
 old_rate←0 ◊ old_pv← +/df[2;]
 rate←guess
 st:
 pv←fin∆df∆net_pv df rate
 new_rate←rate-pv×(-/rate-old_rate)÷pv-old_pv
 old_rate←rate
 rate←new_rate
 old_pv←pv
 →(1<|pv)/st
∇


∇product←df1 fin∆df∆multiply amt
 ⍝ Function to multiply an amount (for example number of shares) by a
 ⍝ dateflow
 product←df1
 product[2;]←df1[2;]×amt
∇

⍝ ∇ix←date fin∆df∆nearest_date df;diff
⍝  ⍝ Function returns the index of the dateflow item closest in time to
⍝  ⍝ the given date (entered as a string.)
⍝  diff←df[1;]-date←date∆lillian date∆US date∆parse date
⍝  ix←((⌊/|diff)=|diff)/⍳⍴diff
⍝ ∇

∇pv←fin∆df∆net_pv args;i;dateflow;investment;outstanding
 ⍝ Function calculates the net present value of a date flow
 ⍝ vector. The first item of dateflow must be the original princiapl
 ⍝ amount (as a negative number) on the date the investment was
 ⍝ purchased. args are dateflow and the interest rate to user.
 dateflow←1⊃args
 i←2⊃args
 dateflow[1;]←dateflow[1;]-dateflow[1;1]
 i←i÷365			⍝ Convert from annual rate.
 pv←+/dateflow[2;]÷(1+i)*dateflow[1;]
∇

∇display←fin∆df∆show df;shape
 ⍝ Function to display a dateflow
 shape←⍴df
 display←'0006/06/06 (55,555,550.10)'⍕((shape[2],3)⍴∊date∆unlillian ¨ df[1;]),df[2;]
∇

∇sum←df1 fin∆df∆subtract df2
 ⍝ Function to subtract df2 from df1
 df2[2;]←-df2[2;]
 sum←df1 fin∆df∆add df2
∇

∇df←fin∆df∆applyInterest args;i;dfi;bal;lb;ix
  ⍝ Function applies an interest rate to a dateflow
  df←1⊃args
  i←(2⊃args)÷365
  dfi←2 0 ⍴0
  bal←df[2;1]
  lb←((1↓⍴df)⍴st),ed
  ix←2
st:
  dfi←dfi fin∆df∆add df[1;ix],-bal×¯1+(1+i)*df[1;ix]-df[1;ix-1]
  bal←bal+df[2;ix]
  →lb[ix←ix+1]
ed:
  df←df fin∆df∆add dfi
∇

⍝ ********************************************************************
⍝			 Portfolio Functions
⍝ ********************************************************************

∇prtf←fin∆prtf∆compile args
 ⍝ Function to compile a portfolio from a vector of dateflows.
 →(1=⍴args)/last
 prtf←(1⊃args) fin∆df∆add fin∆prtf∆compile 1↓args
 →0
 last:
 prtf←1⊃args
 →0
∇

∇array←locale fin∆import∆convertingDates txt;shape;dt;nt;ix
⍝ Function examines an array generated by import∆file∆withDates
⍝ converting any dates to a three element vector of year, month and
⍝ day and converting any numbers to numbers. Locale is a lexicon
⍝ describing the local date conventions (see workspace date).
shape←⍴txt
array←,txt
ix←((⊂locale) datep ¨ array)/⍳⍴array
array[ix]←{date∆lillian locale date∆parse ⍵}¨ array[ix]
array←shape⍴array
∇

∇array← locale fin∆import∆file fname;txt;lines
⍝ Function returns an array from a delimited file.  Each cell is
⍝ tested first to see if it is date and second if it a number. This
⍝ may be too slow for thee. Use import∆file and hope the dates don't
⍝ cause errors.
go1:
⎕es (utl∆numberp txt←utf8∆read fname)/'ERROR READING ',fname
guess:
lines←+/txt=⎕tc[3]
→(import∆delimThreshold>(+/txt=⎕av[10])÷lines)/guess2
array←import∆file∆tab txt
→go2
guess2:
→(import∆delimThreshold>(+/txt=',')÷lines)/ohNo
array←import∆file∆comma txt
→go2
go2:
  array←fin∆import∆convertingNumbers ⊃array
array←locale fin∆import∆convertingDates array
→0
ohNo:
⎕es fname,' IS NOT A DELIMITED FILE'
∇

∇ array←fin∆import∆convertingNumbers org;ix;shape
  ⍝ Function converts strings representing numbers to numbers
  shape←⍴org
  array←,org
  ix←(∊{utl∆numberis ⍵}¨array)/⍳⍴array
  array[ix]←⍎¨array[ix]
  array←shape⍴array
∇

∇Z←fin⍙metadata
 Z←0 2⍴⍬
 Z←Z⍪'Author'          'Bill Daly '
 Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
 Z←Z⍪'Documentation'   'doc/apl-library.info'
 Z←Z⍪'Download'        'https://sourceforge.net/projects/apl-library/'
 Z←Z⍪'License'         'GPL version 3'
 Z←Z⍪'Portability'     'L1'
 Z←Z⍪'Provides'        'Time value of money functions.'
 Z←Z⍪'Requires'        ''
 Z←Z⍪'Version'                           '1 0 2'
 Z←Z⍪'Last update'              '2022-02-07'
∇
