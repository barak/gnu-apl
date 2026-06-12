#!/usr/local/bin/apl --script
⍝ ********************************************************************
⍝
⍝ date 2016-11-29 12:51:06 (GMT-5)
⍝
⍝ ********************************************************************

⍝ date workspace implements lillian dates
⍝ Copyright (C) 2016 Bill Daly

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

)copy_once 5 DALY/utl
)copy_once 3 DALY/lex

∇date←date∆lillian ts;b1;b2;b3;yrs;days;ix
⍝ Function to convert time stamp dates to lillian dates
ts[1]←ts[1]-date∆dates lex∆lookup 'Year 0'
⍝yrs←¯1+⍳ts[1]
yrs←⍳ts[1]
b1←0=400|yrs
b2←0=100|(~b1)/yrs
b3←0=4|(~b2)/(~b1)/yrs
days←(b3×366)+(~b3)×365
days←(b2×365)+(~b2)\days
days←(b1×366)+(~b1)\days
ix←1+365=days[ts[1]]
date←+/¯1↓days
date←date++/(ts[2]-1)↑date∆cal[ix;]
date←date+ts[3]
date←date-date∆dates lex∆lookup 'Pre lillian'
⎕es (date ≤ 0)/'PRE LILLIAN DATE'
∇

∇days←date∆marshalYears ts;yrs;b1;b2;b3
 ⍝ Function to assemble vector of days in each year starting with rarg
 yrs←⍳''⍴ts-date∆dates lex∆lookup 'Year 0'
 b1←0=400|yrs
 b2←0=100|(~b1)/yrs
 b3←0=4|(~b2)/(~b1)/yrs
 days←(b3×366)+(~b3)×365
 days←(b2×365)+(~b2)\days
 days←(b1×366)+(~b1)\days
∇

∇ts←date∆unlillian date;yrs;b1;b2;b3;ix;days
 ⍝ Function to covert a lillian date to a ts formatted date.
 date←date + date∆dates lex∆lookup 'Pre lillian'
 yrs←⍳2+⌈date÷365.25
 b1←0=400|yrs
 b2←0=100|(~b1)/yrs
 b3←0=4|(~b2)/(~b1)/yrs
 days←(b3×366)+(~b3)×365
 days←(b2×365)+(~b2)\days
 days←(b1×366)+(~b1)\days
 ts←3⍴0
 ts[1]←1++/date>+\days
 date←date-+/days[⍳ts[1]-1]
 ix←1+365=days[ts[1]]
 ts[2]←+/date>+\date∆cal[ix;]
 ts[3]←date-+/date∆cal[ix;⍳ts[2]]
 ts[1]←ts[1] + date∆dates lex∆lookup 'Year 0'
 ts[2]←ts[2]+1
∇

∇ yr←date∆lillian∆year date
  ⍝ Function returns the year of a lillian date.
  yr←1⊃date∆unlillian date
∇

∇ b←date∆lillianp date
  ⍝ Function tests to see if date is a lillian date.
  →(~b←utl∆numberp date)/end
  →(~b←0=⍴⍴date)/end
  b←utl∆integerp date
end: b←''⍴b
∇

∇ b←date∆tsp date
  ⍝ Function test to see if this is a time--stamp date
  →(~b←utl∆numberp date)/0
  →(~b←1=⍴⍴date)/0
  →(~b←3=⍴date)/0
  →(~b←∧/utl∆integerp date)/0
  →(~b←date[1]≥⍬⍴date∆US lex∆lookup 'epoch')/0
  →(~b←date[2]∊⍳12)/0
  b←date[3]≤date∆cal[⎕io+1⌊4|date[1];date[2]]
∇

∇ts←locale date∆parse str;num;epoch;max_days
 ⍝ Function to parse a string and return a integer vector of year, month, day.
 ⍝ One ISO 8601 format
 ⍎(utl∆numberp str)/'ts←''NOT TEXT''◊→0'
 str← date∆delim utl∆split (','≠str)/str
 →(∧/~num←utl∆numberis ¨ str)/err
 str[num/⍳⍴num]←⍎,' ',⊃num/str
 →(3=⍴ts←locale date∆parse∆ISO str)/tests
 →(3=⍴ts←locale date∆parse∆long str)/tests
 →(3=⍴ts←locale date∆parse∆short str)/tests
 tests:
 epoch←locale lex∆lookup 'epoch'
 →(ts[1]=0)/er2		⍝ Year tests failed
 →((ts[1]=epoch[1])^ts[2]<epoch[2])/er2
 →(∧/(ts[1 2]=epoch[1 2]),ts[3]<epoch[3])/er2
 →((ts[2]<1)∨ts[2]>12)/err
 max_days←(locale lex∆lookup 'days')[ts[2]]
 max_days←max_days+date∆US date∆parse∆leap_day ts
 →((ts[3]<1)∨ts[3]>max_days)/err
 →0
 err:
 ts←'NOT A DATE'
 →0
 er2:
 ts←'DATE BEFORE EPOCH STARTING ','0006-06-06'⍕epoch
 →0
∇

∇dt←locale date∆parse∆ISO txt
 ⍝ Function attempts to convert text in an ISO 8601 format to a date
 ⍝ made up of year month day.
 dt←3⍴0
 →(3=⍴txt)/extended
 →(1≠⍴txt)/err
 basic_iso:
 dt[1]←⌊txt÷10000
 txt←10000 | txt
 dt[2]←⌊txt÷100
 dt[3]←100 | txt
 →0
 extended:
 →(3≠+/utl∆numberp ¨ txt)/err
 →(0=⍴dt[1]←locale date∆test∆year txt)/err
 →(dt[1]=txt[1])/iso_date
 dt[2 3]←txt[(locale lex∆lookup 'month_pos'),locale lex∆lookup 'day_pos']
 →0
 iso_date:
 dt←txt
 →0
 err:
 dt←''
 →0
∇

∇dt←locale date∆parse∆leap_day ts;leap_month
 ⍝ Function returns 1 if the leap-month number of days should be incremented.
 leap_month←locale lex∆lookup 'leap-month'
 →(~dt←ts[2]=leap_month)/0
 →(dt←0=400|ts[1])/0
 →(~dt←0≠100|ts[1])/0
 dt←0=4|ts[1]
∇

∇dt←locale date∆parse∆long txt;b;mo
 ⍝ Function attempts to convert test in a long, spelled out form to a
 ⍝ date made up of year month day.
   dt←3⍴0
   b←utl∆numberp ¨ txt
   →b[1]/err
   →(13=mo←(locale lex∆lookup 'months') utl∆listSearch utl∆lower ⊃txt[1])/err
   dt[2]←mo
   →(∨/~b[2 3])/err
   dt[1]←txt[3]
   dt[3]←txt[2]
   →0
 err:
 dt←''
 →0
∇

∇dt←locale date∆parse∆short txt;m;mo;b
   ⍝ Function attempts to convert text in a short abbreviate form to a
   ⍝ date made up of year month day.
   dt←3⍴0
   m←locale lex∆lookup 'MTH'
   b←utl∆numberp ¨ txt
   →b[1]/mil
   →(0=⍴mo←m utl∆listSearch utl∆upper ⊃txt[1])/mil
   dt[2]←mo
   →(∨/~b[2 3])/mil
   dt[1]←txt[3]
   dt[3]←txt[2]
 →0
 mil:				⍝ Try military format
   →(0=⍴mo←m utl∆listSearch utl∆upper ⊃txt[2])/err
   dt[2] ← mo
   →(∨/~b[1 3])/err
   dt[1]←txt[3]
   dt[3]←txt[1]
   →0
 err:
   dt←''
   →0
∇

∇b←locale datep dt
  ⍝ Function tests that its argument is a valid date. The data may be
  ⍝ either a character string or a three item vector of year month day.
  →(utl∆numberp dt)/numbers
  b←utl∆numberp dt←locale date∆parse dt
  →0
  numbers:
  →(~b←⍬⍴1=⍴⍴dt)/0
  →(~b←⍬⍴3=⍴dt)/0
  →(~b←dt[1]≥(locale lex∆lookup 'epoch')[1])/0
  →(~b←(dt[2]≥1)∧dt[2]≤12)/0
  b←(dt[3]≥1)∧dt[3]≤(locale lex∆lookup 'days')[3]
  →0
∇

∇yr←locale date∆test∆year dt;e;t
 ⍝ Function called when all three elements of a date are numeric to
 ⍝ determine what the year is.
 yr←dt[1]			⍝ We try ISO dates first
 →(yr≥e←1↑locale lex∆lookup 'epoch')/0
 yr←dt[locale lex∆lookup 'year_pos']
 →(yr≥e)/0
 →((dt[1]>1000)∨yr≥100)/err
 t←locale lex∆lookup 'two-digit-cutoff'
 yr←yr+(1900 2000)[1+(yr>0)∧yr<t]
 →0
 err:
 yr←0
 →0
∇

∇day←locale date∆weekday ts
  ⍝ Function returns the day of the week for a given date.
  ⍎(0≠⍴⍴ts)/'ts←date∆lillian ts'
  day←((7×day<0)+day←¯2+7|ts)⊃locale lex∆lookup 'weekdays'
∇
  
∇day←locale date∆wkd ts
  ⍝ Function returns the day of the week for a given date.
  ⍎(0≠⍴⍴ts)/'ts←date∆lillian ts'
  day←((7×day<0)+day←¯2+7|ts)⊃locale lex∆lookup 'wkd'
∇

∇ts←locale date∆time∆parse str;parts;addHours;md;time;msg
  ⍝ Function to parse a date string or date--time string.
  →(2=⍴parts←date∆time∆delim utl∆bifurcate str)/iso_date
  parts←' ' utl∆split utl∆clean str
  →(err items2 items3 items4 items5)[5⌊⍴parts]
err:
  utl∆es 'INVALID DATE'
  →0
items2:
  utl∆es (~utl∆numberp time←date∆time∆parse_time 2⊃parts)/'INVALID TIME'
  ts←(locale date∆parse 1⊃parts),time
  →editChecks
items3:
  ts←locale date∆parse str
  →editChecks
items4:
  utl∆es (~utl∆numberp time←date∆time∆parse_time 4⊃parts)/'INVALID TIME'
  ts←(locale date∆parse ' ' utl∆join 3↑parts),time
  →editChecks
items5:
  utl∆es (~utl∆numberp time←date∆time∆parse_time 4⊃parts)/'INVALID TIME'
  ts←(locale date∆parse ' ' utl∆join 3↑parts),time
  addHours←((2⊃date∆time∆M) utl∆stringEquals 5⊃parts),(1⊃date∆time∆M) utl∆stringEquals 5⊃parts
  →(~∨/addHours)/err
  ts←ts+(⍴ts)↑0 0 0, +/12 0×addHours
  →editChecks
iso_date:
  ts←(locale date∆parse 1⊃parts),date∆time∆parse_time 2⊃parts
editChecks:
  utl∆es (0≠⍴msg)/msg←,⊃(utl∆stringp ¨ ts)/ts
  md←date∆cal date∆max_days ts
  utl∆es (∨/9999 12 md 24 60 60 <6↑ts)/'TIME ELEMENT TOO LARGE.'
  utl∆es (∨/0>ts)/'TIME ELEMENT TOO SMALL'
∇

∇ts←locale date∆time∆parse_time str;utc_offset;utc_change
  ⍝ Function to parse a time string
  utc_offset←+/∧\~str∊'Z+-'
  ts←':'utl∆split utc_offset↑str
  →(1=⍴ts)/noDelim
Delim:
  ts←⍎¨ts
  →utc
noDelim:
  ts←1⊃ts
  ⍎(6=⍴ts)/'ts←⍎1 1 0 1 1 0 1 1\ts ◊ →utc'
  ⍎(4=⍴ts)/'ts←3↑⍎1 1 0 1 1\ts ◊ →utc'
  ⍎(2=⍴ts)/'ts←3↑⍎ts ◊ →utc'
  →err
 utc:
  →(utc_offset=⍴str)/zero
  →(zero,plus,minus,zero)['Z+-'⍳str[utc_offset+1]]
zero:				⍝ adjustment to hours for UTC
  →0
plus:				⍝ increase hours for UTC
  →(~utl∆numberis utc_change←(utc_offset+1)↓str)/err
  ts[1]←ts[1]+⍎'0',utc_change
  →0
minus:				⍝ decrease hours for UTC
  →(~utl∆numberis utc_change←(utc_offset+1)↓str)/err
  ts[1]←ts[1]-⍎'0',utc_change
  →0
err:
  ts←⊂'BAD TIME ZONE DESIGNATOR'
  →0
∇
⍝ ********************************************************************
⍝ Format time stamps (integers of year month day hour min sec ..
⍝ ********************************************************************

∇str←locale date∆fmt∆3numbers ts;tmp
  ⍝ Function returns a string with three numbers from the time stamp
  tmp←3⍴0
  tmp[∊{locale lex∆lookup ⍵} ¨ 'year_pos' 'month_pos' 'day_pos']←3↑ts
  str←'06/06/0006'⍕tmp
∇

∇str←locale date∆fmt∆6numbers ts;am_pm;tmp
  ⍝ Function returns the date and time as propoerly formatted numbers.
  →(ts[4]≤12)/am
  am_pm←' pm'
  ts[4]←ts[4]-12
  →fmt
am:
  am_pm←' am'
  →fmt
fmt:  
  tmp←6⍴0
  tmp[({locale lex∆lookup ,⍵} ¨ 'year_pos' 'month_pos' 'day_pos'),4 5 6]←6↑ts
  str←('06/06/0006 06:06:06'⍕tmp),am_pm
∇

∇days←cal date∆max_days ts
  ⍝ Function returns the maximum number of days in the month of ts
  days←cal[1+1≠4|ts[1];12⌊ts[2]]
∇

∇tz←date∆tz∆standard∆offset zone;ix
  ⍝ Function returns the offset from Universal Time from the tz
  ⍝ database for standard time.
  utl∆es ((1↑⍴date∆tz)<ix←date∆tz[;1] utl∆listSearch zone)/zone,' NOT FOUND.'
  tz←+/date∆tz[ix;2 3]÷1 60
∇

∇tz←date∆tz∆daylight∆offset zone;ix
  ⍝ Function returns the offset from Universal Time from the tz
  ⍝ database for daylight savings time.
  utl∆es ((1↑⍴date∆tz)<ix←date∆tz[;1] utl∆listSearch zone)/zone,' NOT FOUND.'
  tz←+/date∆tz[ix;4 5]÷1 60
∇

∇ days←date∆daysOfMonth date
  ⍝ Function returns the number of days in the month of the given date.
  utl∆es (utl∆stringp date)/date,' is not a date.'
  utl∆es (1≠⍴⍴date)/(⍕date),' is not a date.'
  ⍎(date∆lillianp date)/'date←date∆unlillian date'
  utl∆es (3≠⍴date←,date)/(⍕date),' is not a date.'
  utl∆es (∨/~utl∆integerp ¨ date)/(⍕date),' is not a date.'
  utl∆es ((date[2]<1)∨date[2]>12)/(⍕date),' is not a date.'
  days←date∆cal[⎕io+1⌊4|date[1];date[2]]
∇

∇Z←date⍙metadata
  Z←0 2⍴⍬
  Z←Z⍪'Author'          'Bill Daly'
  Z←Z⍪'BugEmail'        'bugs@dalywebandedit.com'
  Z←Z⍪'Documentation'   'doc/apl-library.info'
  Z←Z⍪'Download'        'https://sourceforge.net/p/apl-library/code/ci/master/tree/date.apl'
  Z←Z⍪'License'         'GPL v3.0'
  Z←Z⍪'Portability'     'L1'
  Z←Z⍪'Provides'        ''
  Z←Z⍪'Requires'        'utl' 'lex'
  Z←Z⍪'Version'                           '0 2 1'
  Z←Z⍪'Last update'              '2022-02-07'
∇
date∆US ← lex∆init
  date∆US←date∆US lex∆assign 'months' ('january' 'february' 'march' 'april' 'may' 'june' 'july' 'august' 'september' 'october' 'november' 'december')
  date∆US←date∆US lex∆assign 'MTH' ('JAN' 'FEB' 'MAR' 'APR' 'MAY' 'JUN' 'JUL' 'AUG' 'SEP' 'OCT' 'NOV' 'DEC')
  ⍝ weekdays
  date∆US←date∆US lex∆assign 'weekdays' ('sunday' 'monday' 'tuesday' 'wednesday' 'thursday' 'friday' 'saturday')
  ⍝ wkd (Abbreviated weekdays
  date∆US←date∆US lex∆assign 'wkd' ('SUN' 'MON' 'TUE' 'WED' 'THU' 'FRI' 'SAT')
  ⍝ days (for each month not in leap year)
  date∆US←date∆US lex∆assign 'days' (31 28 31 30 31 30 31 31 30 31 30 31)
  ⍝ two-digit-cutoff
  date∆US←date∆US lex∆assign 'two-digit-cutoff' 50
  ⍝ leap-month
  date∆US←date∆US lex∆assign 'leap-month' 2
  ⍝ month_pos
  date∆US←date∆US lex∆assign 'month_pos' 1
  ⍝ epoch; day one of the Gregorian Calendar even though in Pennsylvania
  ⍝ it was March 25, 1752.
  date∆US←date∆US lex∆assign 'epoch' (1582 10 13)
  ⍝ year_pos (ition in date)
  date∆US←date∆US lex∆assign 'year_pos' 3
  ⍝ day_pos
  date∆US←date∆US lex∆assign 'day_pos' 2

date∆dates← lex∆from_alist 'Year 0' 1200 'Pre lillian' 139444



date∆delim←'/\- ,'

date∆cal←31,29 28,2 10 ⍴ 31 30 31 30 31 31 30 31 30 31

date∆time∆M←'am' 'pm'

date∆time∆delim←'T'

date∆time∆utce←'Z'

date∆tz←375 5⍴0 ⍝ prolog ≡1
date∆tz[2;]←(⊂'Africa/Abidjan'),0 0 0 0  
        date∆tz[3;]←(⊂'Africa/Accra'),0 0 0 0  
        date∆tz[4;]←(⊂'Africa/Algiers'),1 0 1 0  
        date∆tz[5;]←(⊂'Africa/Bissau'),0 0 0 0  
        date∆tz[6;]←(⊂'Africa/Cairo'),2 0 2 0  
        date∆tz[7;]←(⊂'Africa/Casablanca'),1 0 1 0  
        date∆tz[8;]←(⊂'Africa/Ceuta'),1 0 2 0  
        date∆tz[9;]←(⊂'Africa/El_Aaiun'),0 0 1 0  
        date∆tz[10;]←(⊂'Africa/Johannesburg'),2 0 2 0  
        date∆tz[11;]←(⊂'Africa/Juba'),3 0 3 0  
        date∆tz[12;]←(⊂'Africa/Khartoum'),2 0 2 0  
        date∆tz[13;]←(⊂'Africa/Lagos'),1 0 1 0  
        date∆tz[14;]←(⊂'Africa/Maputo'),2 0 2 0  
        date∆tz[15;]←(⊂'Africa/Monrovia'),0 0 0 0  
        date∆tz[16;]←(⊂'Africa/Nairobi'),3 0 3 0  
        date∆tz[17;]←(⊂'Africa/Ndjamena'),1 0 1 0  
        date∆tz[18;]←(⊂'Africa/Tripoli'),2 0 2 0  
        date∆tz[19;]←(⊂'Africa/Tunis'),1 0 1 0  
        date∆tz[20;]←(⊂'Africa/Windhoek'),2 0 2 0  
        date∆tz[21;]←(⊂'America/Adak'),¯10 0 ¯9 0  
        date∆tz[22;]←(⊂'America/Anchorage'),¯9 0 ¯8 0  
        date∆tz[23;]←(⊂'America/Araguaina'),¯3 0 ¯3 0  
        date∆tz[24;]←(⊂'America/Argentina/Buenos_Aires'),¯3 0 ¯3 0  
        date∆tz[25;]←(⊂'America/Argentina/Catamarca'),¯3 0 ¯3 0  
        date∆tz[26;]←(⊂'America/Argentina/Cordoba'),¯3 0 ¯3 0  
        date∆tz[27;]←(⊂'America/Argentina/Jujuy'),¯3 0 ¯3 0  
        date∆tz[28;]←(⊂'America/Argentina/La_Rioja'),¯3 0 ¯3 0  
        date∆tz[29;]←(⊂'America/Argentina/Mendoza'),¯3 0 ¯3 0  
        date∆tz[30;]←(⊂'America/Argentina/Rio_Gallegos'),¯3 0 ¯3 0  
        date∆tz[31;]←(⊂'America/Argentina/Salta'),¯3 0 ¯3 0  
        date∆tz[32;]←(⊂'America/Argentina/San_Juan'),¯3 0 ¯3 0  
        date∆tz[33;]←(⊂'America/Argentina/San_Luis'),¯3 0 ¯3 0  
        date∆tz[34;]←(⊂'America/Argentina/Tucuman'),¯3 0 ¯3 0  
        date∆tz[35;]←(⊂'America/Argentina/Ushuaia'),¯3 0 ¯3 0  
        date∆tz[36;]←(⊂'America/Asuncion'),¯4 0 ¯3 0  
        date∆tz[37;]←(⊂'America/Atikokan'),¯5 0 ¯5 0  
        date∆tz[38;]←(⊂'America/Bahia'),¯3 0 ¯3 0  
        date∆tz[39;]←(⊂'America/Bahia_Banderas'),¯6 0 ¯5 0  
        date∆tz[40;]←(⊂'America/Barbados'),¯4 0 ¯4 0  
        date∆tz[41;]←(⊂'America/Belem'),¯3 0 ¯3 0  
        date∆tz[42;]←(⊂'America/Belize'),¯6 0 ¯6 0  
        date∆tz[43;]←(⊂'America/Blanc-Sablon'),¯4 0 ¯4 0  
        date∆tz[44;]←(⊂'America/Boa_Vista'),¯4 0 ¯4 0  
        date∆tz[45;]←(⊂'America/Bogota'),¯5 0 ¯5 0  
        date∆tz[46;]←(⊂'America/Boise'),¯7 0 ¯6 0  
        date∆tz[47;]←(⊂'America/Cambridge_Bay'),¯7 0 ¯6 0  
        date∆tz[48;]←(⊂'America/Campo_Grande'),¯4 0 ¯3 0  
        date∆tz[49;]←(⊂'America/Cancun'),¯5 0 ¯5 0  
        date∆tz[50;]←(⊂'America/Caracas'),¯4 0 ¯4 0  
        date∆tz[51;]←(⊂'America/Cayenne'),¯3 0 ¯3 0  
        date∆tz[52;]←(⊂'America/Chicago'),¯6 0 ¯5 0  
        date∆tz[53;]←(⊂'America/Chihuahua'),¯7 0 ¯6 0  
        date∆tz[54;]←(⊂'America/Costa_Rica'),¯6 0 ¯6 0  
        date∆tz[55;]←(⊂'America/Creston'),¯7 0 ¯7 0  
        date∆tz[56;]←(⊂'America/Cuiaba'),¯4 0 ¯3 0  
        date∆tz[57;]←(⊂'America/Curacao'),¯4 0 ¯4 0  
        date∆tz[58;]←(⊂'America/Danmarkshavn'),0 0 0 0  
        date∆tz[59;]←(⊂'America/Dawson'),¯8 0 ¯7 0  
        date∆tz[60;]←(⊂'America/Dawson_Creek'),¯7 0 ¯7 0  
        date∆tz[61;]←(⊂'America/Denver'),¯7 0 ¯6 0  
        date∆tz[62;]←(⊂'America/Detroit'),¯5 0 ¯4 0  
        date∆tz[63;]←(⊂'America/Edmonton'),¯7 0 ¯6 0  
        date∆tz[64;]←(⊂'America/Eirunepe'),¯5 0 ¯5 0  
        date∆tz[65;]←(⊂'America/El_Salvador'),¯6 0 ¯6 0  
        date∆tz[66;]←(⊂'America/Fort_Nelson'),¯7 0 ¯7 0  
        date∆tz[67;]←(⊂'America/Fortaleza'),¯3 0 ¯3 0  
        date∆tz[68;]←(⊂'America/Glace_Bay'),¯4 0 ¯3 0  
        date∆tz[69;]←(⊂'America/Godthab'),¯3 0 ¯2 0  
        date∆tz[70;]←(⊂'America/Goose_Bay'),¯4 0 ¯3 0  
        date∆tz[71;]←(⊂'America/Grand_Turk'),¯5 0 ¯4 0  
        date∆tz[72;]←(⊂'America/Guatemala'),¯6 0 ¯6 0  
        date∆tz[73;]←(⊂'America/Guayaquil'),¯5 0 ¯5 0  
        date∆tz[74;]←(⊂'America/Guyana'),¯4 0 ¯4 0  
        date∆tz[75;]←(⊂'America/Halifax'),¯4 0 ¯3 0  
        date∆tz[76;]←(⊂'America/Havana'),¯5 0 ¯4 0  
        date∆tz[77;]←(⊂'America/Hermosillo'),¯7 0 ¯7 0  
        date∆tz[78;]←(⊂'America/Indiana/Indianapolis'),¯5 0 ¯4 0  
        date∆tz[79;]←(⊂'America/Indiana/Knox'),¯6 0 ¯5 0  
        date∆tz[80;]←(⊂'America/Indiana/Marengo'),¯5 0 ¯4 0  
        date∆tz[81;]←(⊂'America/Indiana/Petersburg'),¯5 0 ¯4 0  
        date∆tz[82;]←(⊂'America/Indiana/Tell_City'),¯6 0 ¯5 0  
        date∆tz[83;]←(⊂'America/Indiana/Vevay'),¯5 0 ¯4 0  
        date∆tz[84;]←(⊂'America/Indiana/Vincennes'),¯5 0 ¯4 0  
        date∆tz[85;]←(⊂'America/Indiana/Winamac'),¯5 0 ¯4 0  
        date∆tz[86;]←(⊂'America/Inuvik'),¯7 0 ¯6 0  
        date∆tz[87;]←(⊂'America/Iqaluit'),¯5 0 ¯4 0  
        date∆tz[88;]←(⊂'America/Jamaica'),¯5 0 ¯5 0  
        date∆tz[89;]←(⊂'America/Juneau'),¯9 0 ¯8 0  
        date∆tz[90;]←(⊂'America/Kentucky/Louisville'),¯5 0 ¯4 0  
        date∆tz[91;]←(⊂'America/Kentucky/Monticello'),¯5 0 ¯4 0  
        date∆tz[92;]←(⊂'America/La_Paz'),¯4 0 ¯4 0  
        date∆tz[93;]←(⊂'America/Lima'),¯5 0 ¯5 0  
        date∆tz[94;]←(⊂'America/Los_Angeles'),¯8 0 ¯7 0  
        date∆tz[95;]←(⊂'America/Maceio'),¯3 0 ¯3 0  
        date∆tz[96;]←(⊂'America/Managua'),¯6 0 ¯6 0  
        date∆tz[97;]←(⊂'America/Manaus'),¯4 0 ¯4 0  
        date∆tz[98;]←(⊂'America/Martinique'),¯4 0 ¯4 0  
        date∆tz[99;]←(⊂'America/Matamoros'),¯6 0 ¯5 0  
        date∆tz[100;]←(⊂'America/Mazatlan'),¯7 0 ¯6 0  
        date∆tz[101;]←(⊂'America/Menominee'),¯6 0 ¯5 0  
        date∆tz[102;]←(⊂'America/Merida'),¯6 0 ¯5 0  
        date∆tz[103;]←(⊂'America/Metlakatla'),¯9 0 ¯8 0  
        date∆tz[104;]←(⊂'America/Mexico_City'),¯6 0 ¯5 0  
        date∆tz[105;]←(⊂'America/Miquelon'),¯3 0 ¯2 0  
        date∆tz[106;]←(⊂'America/Moncton'),¯4 0 ¯3 0  
        date∆tz[107;]←(⊂'America/Monterrey'),¯6 0 ¯5 0  
        date∆tz[108;]←(⊂'America/Montevideo'),¯3 0 ¯3 0  
        date∆tz[109;]←(⊂'America/Nassau'),¯5 0 ¯4 0  
        date∆tz[110;]←(⊂'America/New_York'),¯5 0 ¯4 0  
        date∆tz[111;]←(⊂'America/Nipigon'),¯5 0 ¯4 0  
        date∆tz[112;]←(⊂'America/Nome'),¯9 0 ¯8 0  
        date∆tz[113;]←(⊂'America/Noronha'),¯2 0 ¯2 0  
        date∆tz[114;]←(⊂'America/North_Dakota/Beulah'),¯6 0 ¯5 0  
        date∆tz[115;]←(⊂'America/North_Dakota/Center'),¯6 0 ¯5 0  
        date∆tz[116;]←(⊂'America/North_Dakota/New_Salem'),¯6 0 ¯5 0  
        date∆tz[117;]←(⊂'America/Ojinaga'),¯7 0 ¯6 0  
        date∆tz[118;]←(⊂'America/Panama'),¯5 0 ¯5 0  
        date∆tz[119;]←(⊂'America/Pangnirtung'),¯5 0 ¯4 0  
        date∆tz[120;]←(⊂'America/Paramaribo'),¯3 0 ¯3 0  
        date∆tz[121;]←(⊂'America/Phoenix'),¯7 0 ¯7 0  
        date∆tz[122;]←(⊂'America/Port_of_Spain'),¯4 0 ¯4 0  
        date∆tz[123;]←(⊂'America/Port-au-Prince'),¯5 0 ¯4 0  
        date∆tz[124;]←(⊂'America/Porto_Velho'),¯4 0 ¯4 0  
        date∆tz[125;]←(⊂'America/Puerto_Rico'),¯4 0 ¯4 0  
        date∆tz[126;]←(⊂'America/Punta_Arenas'),¯3 0 ¯3 0  
        date∆tz[127;]←(⊂'America/Rainy_River'),¯6 0 ¯5 0  
        date∆tz[128;]←(⊂'America/Rankin_Inlet'),¯6 0 ¯5 0  
        date∆tz[129;]←(⊂'America/Recife'),¯3 0 ¯3 0  
        date∆tz[130;]←(⊂'America/Regina'),¯6 0 ¯6 0  
        date∆tz[131;]←(⊂'America/Resolute'),¯6 0 ¯5 0  
        date∆tz[132;]←(⊂'America/Rio_Branco'),¯5 0 ¯5 0  
        date∆tz[133;]←(⊂'America/Santarem'),¯3 0 ¯3 0  
        date∆tz[134;]←(⊂'America/Santiago'),¯4 0 ¯3 0  
        date∆tz[135;]←(⊂'America/Santo_Domingo'),¯4 0 ¯4 0  
        date∆tz[136;]←(⊂'America/Sao_Paulo'),¯3 0 ¯3 0  
        date∆tz[137;]←(⊂'America/Scoresbysund'),¯1 0 0 0  
        date∆tz[138;]←(⊂'America/Sitka'),¯9 0 ¯8 0  
        date∆tz[139;]←(⊂'America/St_Johns'),¯3 30 ¯2 30  
        date∆tz[140;]←(⊂'America/Swift_Current'),¯6 0 ¯6 0  
        date∆tz[141;]←(⊂'America/Tegucigalpa'),¯6 0 ¯6 0  
        date∆tz[142;]←(⊂'America/Thule'),¯4 0 ¯3 0  
        date∆tz[143;]←(⊂'America/Thunder_Bay'),¯5 0 ¯4 0  
        date∆tz[144;]←(⊂'America/Tijuana'),¯8 0 ¯7 0  
        date∆tz[145;]←(⊂'America/Toronto'),¯5 0 ¯4 0  
        date∆tz[146;]←(⊂'America/Vancouver'),¯8 0 ¯7 0  
        date∆tz[147;]←(⊂'America/Whitehorse'),¯8 0 ¯7 0  
        date∆tz[148;]←(⊂'America/Winnipeg'),¯6 0 ¯5 0  
        date∆tz[149;]←(⊂'America/Yakutat'),¯9 0 ¯8 0  
        date∆tz[150;]←(⊂'America/Yellowknife'),¯7 0 ¯6 0  
        date∆tz[151;]←(⊂'Antarctica/Casey'),11 0 11 0  
        date∆tz[152;]←(⊂'Antarctica/Davis'),7 0 7 0  
        date∆tz[153;]←(⊂'Antarctica/DumontDUrville'),10 0 10 0  
        date∆tz[154;]←(⊂'Antarctica/Macquarie'),11 0 11 0  
        date∆tz[155;]←(⊂'Antarctica/Mawson'),5 0 5 0  
        date∆tz[156;]←(⊂'Antarctica/Palmer'),¯3 0 ¯3 0  
        date∆tz[157;]←(⊂'Antarctica/Rothera'),¯3 0 ¯3 0  
        date∆tz[158;]←(⊂'Antarctica/Syowa'),3 0 3 0  
        date∆tz[159;]←(⊂'Antarctica/Troll'),0 0 2 0  
        date∆tz[160;]←(⊂'Antarctica/Vostok'),6 0 6 0  
        date∆tz[161;]←(⊂'Asia/Almaty'),6 0 6 0  
        date∆tz[162;]←(⊂'Asia/Amman'),2 0 3 0  
        date∆tz[163;]←(⊂'Asia/Anadyr'),12 0 12 0  
        date∆tz[164;]←(⊂'Asia/Aqtau'),5 0 5 0  
        date∆tz[165;]←(⊂'Asia/Aqtobe'),5 0 5 0  
        date∆tz[166;]←(⊂'Asia/Ashgabat'),5 0 5 0  
        date∆tz[167;]←(⊂'Asia/Atyrau'),5 0 5 0  
        date∆tz[168;]←(⊂'Asia/Baghdad'),3 0 3 0  
        date∆tz[169;]←(⊂'Asia/Baku'),4 0 4 0  
        date∆tz[170;]←(⊂'Asia/Bangkok'),7 0 7 0  
        date∆tz[171;]←(⊂'Asia/Barnaul'),7 0 7 0  
        date∆tz[172;]←(⊂'Asia/Beirut'),2 0 3 0  
        date∆tz[173;]←(⊂'Asia/Bishkek'),6 0 6 0  
        date∆tz[174;]←(⊂'Asia/Brunei'),8 0 8 0  
        date∆tz[175;]←(⊂'Asia/Chita'),9 0 9 0  
        date∆tz[176;]←(⊂'Asia/Choibalsan'),8 0 8 0  
        date∆tz[177;]←(⊂'Asia/Colombo'),5 30 5 30  
        date∆tz[178;]←(⊂'Asia/Damascus'),2 0 3 0  
        date∆tz[179;]←(⊂'Asia/Dhaka'),6 0 6 0  
        date∆tz[180;]←(⊂'Asia/Dili'),9 0 9 0  
        date∆tz[181;]←(⊂'Asia/Dubai'),4 0 4 0  
        date∆tz[182;]←(⊂'Asia/Dushanbe'),5 0 5 0  
        date∆tz[183;]←(⊂'Asia/Famagusta'),2 0 2 0  
        date∆tz[184;]←(⊂'Asia/Gaza'),2 0 3 0  
        date∆tz[185;]←(⊂'Asia/Hebron'),2 0 3 0  
        date∆tz[186;]←(⊂'Asia/Ho_Chi_Minh'),7 0 7 0  
        date∆tz[187;]←(⊂'Asia/Hong_Kong'),8 0 8 0  
        date∆tz[188;]←(⊂'Asia/Hovd'),7 0 7 0  
        date∆tz[189;]←(⊂'Asia/Irkutsk'),8 0 8 0  
        date∆tz[190;]←(⊂'Asia/Jakarta'),7 0 7 0  
        date∆tz[191;]←(⊂'Asia/Jayapura'),9 0 9 0  
        date∆tz[192;]←(⊂'Asia/Jerusalem'),2 0 3 0  
        date∆tz[193;]←(⊂'Asia/Kabul'),4 30 4 30  
        date∆tz[194;]←(⊂'Asia/Kamchatka'),12 0 12 0  
        date∆tz[195;]←(⊂'Asia/Karachi'),5 0 5 0  
        date∆tz[196;]←(⊂'Asia/Kathmandu'),5 45 5 45  
        date∆tz[197;]←(⊂'Asia/Khandyga'),9 0 9 0  
        date∆tz[198;]←(⊂'Asia/Kolkata'),5 30 5 30  
        date∆tz[199;]←(⊂'Asia/Krasnoyarsk'),7 0 7 0  
        date∆tz[200;]←(⊂'Asia/Kuala_Lumpur'),8 0 8 0  
        date∆tz[201;]←(⊂'Asia/Kuching'),8 0 8 0  
        date∆tz[202;]←(⊂'Asia/Macau'),8 0 8 0  
        date∆tz[203;]←(⊂'Asia/Magadan'),11 0 11 0  
        date∆tz[204;]←(⊂'Asia/Makassar'),8 0 8 0  
        date∆tz[205;]←(⊂'Asia/Manila'),8 0 8 0  
        date∆tz[206;]←(⊂'Asia/Novokuznetsk'),7 0 7 0  
        date∆tz[207;]←(⊂'Asia/Novosibirsk'),7 0 7 0  
        date∆tz[208;]←(⊂'Asia/Omsk'),6 0 6 0  
        date∆tz[209;]←(⊂'Asia/Oral'),5 0 5 0  
        date∆tz[210;]←(⊂'Asia/Pontianak'),7 0 7 0  
        date∆tz[211;]←(⊂'Asia/Pyongyang'),9 0 9 0  
        date∆tz[212;]←(⊂'Asia/Qatar'),3 0 3 0  
        date∆tz[213;]←(⊂'Asia/Qyzylorda'),5 0 5 0  
        date∆tz[214;]←(⊂'Asia/Riyadh'),3 0 3 0  
        date∆tz[215;]←(⊂'Asia/Sakhalin'),11 0 11 0  
        date∆tz[216;]←(⊂'Asia/Samarkand'),5 0 5 0  
        date∆tz[217;]←(⊂'Asia/Seoul'),9 0 9 0  
        date∆tz[218;]←(⊂'Asia/Shanghai'),8 0 8 0  
        date∆tz[219;]←(⊂'Asia/Singapore'),8 0 8 0  
        date∆tz[220;]←(⊂'Asia/Srednekolymsk'),11 0 11 0  
        date∆tz[221;]←(⊂'Asia/Taipei'),8 0 8 0  
        date∆tz[222;]←(⊂'Asia/Tashkent'),5 0 5 0  
        date∆tz[223;]←(⊂'Asia/Tbilisi'),4 0 4 0  
        date∆tz[224;]←(⊂'Asia/Tehran'),3 30 4 30  
        date∆tz[225;]←(⊂'Asia/Thimphu'),6 0 6 0  
        date∆tz[226;]←(⊂'Asia/Tokyo'),9 0 9 0  
        date∆tz[227;]←(⊂'Asia/Tomsk'),7 0 7 0  
        date∆tz[228;]←(⊂'Asia/Ulaanbaatar'),8 0 8 0  
        date∆tz[229;]←(⊂'Asia/Urumqi'),6 0 6 0  
        date∆tz[230;]←(⊂'Asia/Ust-Nera'),10 0 10 0  
        date∆tz[231;]←(⊂'Asia/Vladivostok'),10 0 10 0  
        date∆tz[232;]←(⊂'Asia/Yakutsk'),9 0 9 0  
        date∆tz[233;]←(⊂'Asia/Yangon'),6 30 6 30  
        date∆tz[234;]←(⊂'Asia/Yekaterinburg'),5 0 5 0  
        date∆tz[235;]←(⊂'Asia/Yerevan'),4 0 4 0  
        date∆tz[236;]←(⊂'Atlantic/Azores'),¯1 0 0 0  
        date∆tz[237;]←(⊂'Atlantic/Bermuda'),¯4 0 ¯3 0  
        date∆tz[238;]←(⊂'Atlantic/Canary'),0 0 1 0  
        date∆tz[239;]←(⊂'Atlantic/Cape_Verde'),¯1 0 ¯1 0  
        date∆tz[240;]←(⊂'Atlantic/Faroe'),0 0 1 0  
        date∆tz[241;]←(⊂'Atlantic/Madeira'),0 0 1 0  
        date∆tz[242;]←(⊂'Atlantic/Reykjavik'),0 0 0 0  
        date∆tz[243;]←(⊂'Atlantic/South_Georgia'),¯2 0 ¯2 0  
        date∆tz[244;]←(⊂'Atlantic/Stanley'),¯3 0 ¯3 0  
        date∆tz[245;]←(⊂'Australia/Adelaide'),9 30 10 30  
        date∆tz[246;]←(⊂'Australia/Brisbane'),10 0 10 0  
        date∆tz[247;]←(⊂'Australia/Broken_Hill'),9 30 10 30  
        date∆tz[248;]←(⊂'Australia/Currie'),10 0 11 0  
        date∆tz[249;]←(⊂'Australia/Darwin'),9 30 9 30  
        date∆tz[250;]←(⊂'Australia/Eucla'),8 45 8 45  
        date∆tz[251;]←(⊂'Australia/Hobart'),10 0 11 0  
        date∆tz[252;]←(⊂'Australia/Lindeman'),10 0 10 0  
        date∆tz[253;]←(⊂'Australia/Lord_Howe'),10 30 11 0  
        date∆tz[254;]←(⊂'Australia/Melbourne'),10 0 11 0  
        date∆tz[255;]←(⊂'Australia/Perth'),8 0 8 0  
        date∆tz[256;]←(⊂'Australia/Sydney'),10 0 11 0  
        date∆tz[257;]←(⊂'Etc/GMT'),0 0 0 0  
        date∆tz[258;]←(⊂'Etc/GMT+1'),¯1 0 ¯1 0  
        date∆tz[259;]←(⊂'Etc/GMT+10'),0 0 0 0  
        date∆tz[260;]←(⊂'Etc/GMT+11'),¯1 0 ¯1 0  
        date∆tz[261;]←(⊂'Etc/GMT+12'),¯2 0 ¯2 0  
        date∆tz[262;]←(⊂'Etc/GMT+2'),¯2 0 ¯2 0  
        date∆tz[263;]←(⊂'Etc/GMT+3'),¯3 0 ¯3 0  
        date∆tz[264;]←(⊂'Etc/GMT+4'),¯4 0 ¯4 0  
        date∆tz[265;]←(⊂'Etc/GMT+5'),¯5 0 ¯5 0  
        date∆tz[266;]←(⊂'Etc/GMT+6'),¯6 0 ¯6 0  
        date∆tz[267;]←(⊂'Etc/GMT+7'),¯7 0 ¯7 0  
        date∆tz[268;]←(⊂'Etc/GMT+8'),¯8 0 ¯8 0  
        date∆tz[269;]←(⊂'Etc/GMT+9'),¯9 0 ¯9 0  
        date∆tz[270;]←(⊂'Etc/GMT-1'),1 0 1 0  
        date∆tz[271;]←(⊂'Etc/GMT-10'),10 0 10 0  
        date∆tz[272;]←(⊂'Etc/GMT-11'),11 0 11 0  
        date∆tz[273;]←(⊂'Etc/GMT-12'),12 0 12 0  
        date∆tz[274;]←(⊂'Etc/GMT-13'),13 0 13 0  
        date∆tz[275;]←(⊂'Etc/GMT-14'),14 0 14 0  
        date∆tz[276;]←(⊂'Etc/GMT-2'),2 0 2 0  
        date∆tz[277;]←(⊂'Etc/GMT-3'),3 0 3 0  
        date∆tz[278;]←(⊂'Etc/GMT-4'),4 0 4 0  
        date∆tz[279;]←(⊂'Etc/GMT-5'),5 0 5 0  
        date∆tz[280;]←(⊂'Etc/GMT-6'),6 0 6 0  
        date∆tz[281;]←(⊂'Etc/GMT-7'),7 0 7 0  
        date∆tz[282;]←(⊂'Etc/GMT-8'),8 0 8 0  
        date∆tz[283;]←(⊂'Etc/GMT-9'),9 0 9 0  
        date∆tz[284;]←(⊂'Etc/UTC'),0 0 0 0  
        date∆tz[285;]←(⊂'Europe/Amsterdam'),1 0 2 0  
        date∆tz[286;]←(⊂'Europe/Andorra'),1 0 2 0  
        date∆tz[287;]←(⊂'Europe/Astrakhan'),4 0 4 0  
        date∆tz[288;]←(⊂'Europe/Athens'),2 0 3 0  
        date∆tz[289;]←(⊂'Europe/Belgrade'),1 0 2 0  
        date∆tz[290;]←(⊂'Europe/Berlin'),1 0 2 0  
        date∆tz[291;]←(⊂'Europe/Brussels'),1 0 2 0  
        date∆tz[292;]←(⊂'Europe/Bucharest'),2 0 3 0  
        date∆tz[293;]←(⊂'Europe/Budapest'),1 0 2 0  
        date∆tz[294;]←(⊂'Europe/Chisinau'),2 0 3 0  
        date∆tz[295;]←(⊂'Europe/Copenhagen'),1 0 2 0  
        date∆tz[296;]←(⊂'Europe/Dublin'),0 0 1 0  
        date∆tz[297;]←(⊂'Europe/Gibraltar'),1 0 2 0  
        date∆tz[298;]←(⊂'Europe/Helsinki'),2 0 3 0  
        date∆tz[299;]←(⊂'Europe/Istanbul'),3 0 3 0  
        date∆tz[300;]←(⊂'Europe/Kaliningrad'),2 0 2 0  
        date∆tz[301;]←(⊂'Europe/Kiev'),2 0 3 0  
        date∆tz[302;]←(⊂'Europe/Kirov'),3 0 3 0  
        date∆tz[303;]←(⊂'Europe/Lisbon'),0 0 1 0  
        date∆tz[304;]←(⊂'Europe/London'),0 0 1 0  
        date∆tz[305;]←(⊂'Europe/Luxembourg'),1 0 2 0  
        date∆tz[306;]←(⊂'Europe/Madrid'),1 0 2 0  
        date∆tz[307;]←(⊂'Europe/Malta'),1 0 2 0  
        date∆tz[308;]←(⊂'Europe/Minsk'),3 0 3 0  
        date∆tz[309;]←(⊂'Europe/Monaco'),1 0 2 0  
        date∆tz[310;]←(⊂'Europe/Moscow'),3 0 3 0  
        date∆tz[311;]←(⊂'Asia/Nicosia'),2 0 3 0  
        date∆tz[312;]←(⊂'Europe/Oslo'),1 0 2 0  
        date∆tz[313;]←(⊂'Europe/Paris'),1 0 2 0  
        date∆tz[314;]←(⊂'Europe/Prague'),1 0 2 0  
        date∆tz[315;]←(⊂'Europe/Riga'),2 0 3 0  
        date∆tz[316;]←(⊂'Europe/Rome'),1 0 2 0  
        date∆tz[317;]←(⊂'Europe/Samara'),4 0 4 0  
        date∆tz[318;]←(⊂'Europe/Saratov'),4 0 4 0  
        date∆tz[319;]←(⊂'Europe/Simferopol'),3 0 3 0  
        date∆tz[320;]←(⊂'Europe/Sofia'),2 0 3 0  
        date∆tz[321;]←(⊂'Europe/Stockholm'),1 0 2 0  
        date∆tz[322;]←(⊂'Europe/Tallinn'),2 0 3 0  
        date∆tz[323;]←(⊂'Europe/Tirane'),1 0 2 0  
        date∆tz[324;]←(⊂'Europe/Ulyanovsk'),4 0 4 0  
        date∆tz[325;]←(⊂'Europe/Uzhgorod'),2 0 3 0  
        date∆tz[326;]←(⊂'Europe/Vienna'),1 0 2 0  
        date∆tz[327;]←(⊂'Europe/Vilnius'),2 0 3 0  
        date∆tz[328;]←(⊂'Europe/Volgograd'),4 0 4 0  
        date∆tz[329;]←(⊂'Europe/Warsaw'),1 0 2 0  
        date∆tz[330;]←(⊂'Europe/Zaporozhye'),2 0 3 0  
        date∆tz[331;]←(⊂'Europe/Zurich'),1 0 2 0  
        date∆tz[332;]←(⊂'Indian/Chagos'),6 0 6 0  
        date∆tz[333;]←(⊂'Indian/Christmas'),7 0 7 0  
        date∆tz[334;]←(⊂'Indian/Cocos'),6 30 6 30  
        date∆tz[335;]←(⊂'Indian/Kerguelen'),5 0 5 0  
        date∆tz[336;]←(⊂'Indian/Mahe'),4 0 4 0  
        date∆tz[337;]←(⊂'Indian/Maldives'),5 0 5 0  
        date∆tz[338;]←(⊂'Indian/Mauritius'),4 0 4 0  
        date∆tz[339;]←(⊂'Indian/Reunion'),4 0 4 0  
        date∆tz[340;]←(⊂'Pacific/Apia'),13 0 14 0  
        date∆tz[341;]←(⊂'Pacific/Auckland'),12 0 13 0  
        date∆tz[342;]←(⊂'Pacific/Bougainville'),11 0 11 0  
        date∆tz[343;]←(⊂'Pacific/Chatham'),12 45 13 45  
        date∆tz[344;]←(⊂'Pacific/Chuuk'),10 0 10 0  
        date∆tz[345;]←(⊂'Pacific/Easter'),¯6 0 ¯5 0  
        date∆tz[346;]←(⊂'Pacific/Efate'),11 0 11 0  
        date∆tz[347;]←(⊂'Pacific/Enderbury'),13 0 13 0  
        date∆tz[348;]←(⊂'Pacific/Fakaofo'),13 0 13 0  
        date∆tz[349;]←(⊂'Pacific/Fiji'),12 0 13 0  
        date∆tz[350;]←(⊂'Pacific/Funafuti'),12 0 12 0  
        date∆tz[351;]←(⊂'Pacific/Galapagos'),¯6 0 ¯6 0  
        date∆tz[352;]←(⊂'Pacific/Gambier'),¯9 0 ¯9 0  
        date∆tz[353;]←(⊂'Pacific/Guadalcanal'),11 0 11 0  
        date∆tz[354;]←(⊂'Pacific/Guam'),10 0 10 0  
        date∆tz[355;]←(⊂'Pacific/Honolulu'),0 0 0 0  
        date∆tz[356;]←(⊂'Pacific/Kiritimati'),14 0 14 0  
        date∆tz[357;]←(⊂'Pacific/Kosrae'),11 0 11 0  
        date∆tz[358;]←(⊂'Pacific/Kwajalein'),12 0 12 0  
        date∆tz[359;]←(⊂'Pacific/Majuro'),12 0 12 0  
        date∆tz[360;]←(⊂'Pacific/Marquesas'),¯9 30 ¯9 30  
        date∆tz[361;]←(⊂'Pacific/Nauru'),12 0 12 0  
        date∆tz[362;]←(⊂'Pacific/Niue'),¯1 0 ¯1 0  
        date∆tz[363;]←(⊂'Pacific/Norfolk'),11 0 11 0  
        date∆tz[364;]←(⊂'Pacific/Noumea'),11 0 11 0  
        date∆tz[365;]←(⊂'Pacific/Pago_Pago'),¯1 0 ¯1 0  
        date∆tz[366;]←(⊂'Pacific/Palau'),9 0 9 0  
        date∆tz[367;]←(⊂'Pacific/Pitcairn'),¯8 0 ¯8 0  
        date∆tz[368;]←(⊂'Pacific/Pohnpei'),11 0 11 0  
        date∆tz[369;]←(⊂'Pacific/Port_Moresby'),10 0 10 0  
        date∆tz[370;]←(⊂'Pacific/Rarotonga'),0 0 0 0  
        date∆tz[371;]←(⊂'Pacific/Tahiti'),0 0 0 0  
        date∆tz[372;]←(⊂'Pacific/Tarawa'),12 0 12 0  
        date∆tz[373;]←(⊂'Pacific/Tongatapu'),13 0 14 0  
        date∆tz[374;]←(⊂'Pacific/Wake'),12 0 12 0  
        date∆tz[375;]←(⊂'Pacific/Wallis'),12 0 12 0  
