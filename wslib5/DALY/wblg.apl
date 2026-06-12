#! /usr/local/bin/apl --script
⍝ ********************************************************************
⍝ web_log.apl Workspace to analyze web server logs
⍝ Copyright (C) 2022 Bill Daly

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
)copy_once 5 DALY/utf8
)copy_once 5 DALY/utl
)copy_once 3 DALY/lex
)copy_once 3 DALY/date

∇ log←wblg∆loadFile fname;raw
  ⍝ Function reads a file and splits it into log records
  raw←utf8∆read fname
  raw←(-⎕tc[3]=¯1↑raw)↓raw
  log←' ' utl∆split_with_quotes ¨ ⎕tc[3] utl∆split raw
∇

⍝ ********************************************************************
⍝ Functions to return a field from a record
⍝ ********************************************************************

∇ client←wblg∆ip record
  ⍝ Function returns the ip address of the client
  client ← 1⊃record
∇

∇ user←wblg∆user record
  ⍝ Function returns the user id
  user← 3⊃ record
∇

∇ time←wblg∆time record
  ⍝ Function returns the time string from a record
  time←1↓4⊃ record
∇

∇ zone←wblg∆zone record
  ⍝ Function returns the time zone.
  zone←¯1↓5⊃record
∇

∇ request←wblg∆request record
  ⍝ Function returns the request made of the web server.
  request←6⊃record
∇

∇ status←wblg∆status record
  ⍝ Function returns the HTTP status of the request
  status ← ⍎ 7 ⊃ record
∇

∇ size← wblg∆size record
  ⍝ Function returns the size in bytes of the server's response
  size← ⍎ 8 ⊃ record
∇

∇ referrer←wblg∆referrer record
  ⍝ Function returns the url of the referrer
  referrer←9 ⊃ record
∇

∇ agent ← wblg∆agent record
  ⍝ Function returns the name of the client's browser
  agent ← 10⊃ record
∇

  
∇ time←date_cf wblg∆ParsedTime record;zone
  ⍝ Function returns the time as a vector of five digits
  time←wblg∆time record
  time←(∧\time≠':')/time
  time←date_cf date∆parse time
∇

∇ method←wblg∆method record
  ⍝ Function returns the HTTP method of the request
  method← 1⊃ ' ' utl∆split wblg∆request record
∇

∇ target←wblg∆target record
  ⍝ Function returns the target page of the request
  target← 2⊃ ' ' utl∆split wblg∆request record
∇

∇fname←wblg∆called_function record
  ⍝ Function returns everything in the target to the left of '?'
  fname←(∧\'?'≠fname)/fname←wblg∆target record
∇

⍝ ********************************************************************
⍝ Predicates
⍝ ********************************************************************

∇b←ip wblg∆test_ip record;ix
  ⍝ Function tests the client ip address against a list of ip
  ⍝ addresses
  ⍎(1=≡ip)/'ip←⊂ip'
  ix←wblg∆ip record
  b←∨/∊{ix utl∆stringEquals ⍵}¨ip
∇


∇ b←suffix wblg∆test_suffix  record;fname
  ⍝ Function tests if the target file has a suffix in the left
  ⍝ argument list.
  ⍎(1=≡suffix)/'suffix←⊂suffix'
  fname←wblg∆called_function record
  b←∨/{∧/⍵=(-⍴⍵)↑ fname}¨suffix
∇

∇ b←str wblg∆test_agent_contents record;referrer;len;height
  ⍝ Function looks for the left argument in the agent field.
  ⍎(1=≡str)/'str←⊂str'
  referrer←wblg∆agent record
  len←⍴referrer ◊ height←⍴¨str
  b←∨/height {∨/⍵∧.=(¯1+⍳⍺)⌽(⍺,len)⍴referrer}¨str
∇

∇ b←fname wblg∆test_file record
  ⍝ Function test if this is a request for a specific file
  b←fname utl∆stringEquals wblg∆called_function record
∇

∇ b←status wblg∆test_status record
  ⍝ Function tests if this is the return code in the record
  b← status = wblg∆status record
∇

∇b←host wblg∆test_host record;pre;ref;len
  ⍝ Predicate tests for a host name in the referrer field.
  pre←+/∧\(1↑host)≠ref←wblg∆referrer record
  len←⍴host
  b←∧/host=len↑pre↓ref
∇

∇ b← wblg∆test_not_referred record;ref
  ⍝ Predicates test for a referral field
  b←('-'=1↑ref)∧1=⍴ref←,wblg∆referrer record
∇

⍝ ********************************************************************
⍝ Filters
⍝ ********************************************************************

∇new←larg (pred wblg∆filter) old
  ⍝ Operator to apply a predicate to an array of log records.
  →(0=⎕nc'larg')/mo
  new←(∊{larg pred ⍵}¨old)/old
  →0
mo:
  new←(∊{pred ⍵}¨old)/old
  →0
∇

∇ new←larg (pred wblg∆not_filter) old
  ⍝ Operator to apply a predicate to an array and return records that
  ⍝ don't match.
  →(0=⎕nc'larg')/mo
  new←(~∊{larg pred ⍵}¨old)/old
  →0
mo:
  new←(~∊{pred ⍵}¨old)/old
  →0
∇

∇ new←ip wblg∆filter_hits old
  ⍝ Function returns the number of page requests
  new←200 wblg∆test_status wblg∆filter old
  new←'bot' 'spider' 'google' 'crawler' wblg∆test_agent_contents wblg∆not_filter new
  new←'js' 'css' 'jpg' 'gif' 'ico' 'png' wblg∆test_suffix wblg∆not_filter new
  new←ip wblg∆test_ip wblg∆not_filter new
  new←'/wordpress1/wp-cron.php' wblg∆test_file wblg∆not_filter new
∇

∇ new←wblg∆filter_success old
  ⍝ Function returns log records with status code 200
  new← 200 wblg∆test_status wblg∆filter old
∇

∇ new←me wblg∆filter_not_me old
  ⍝ Function removes administrative records based on IP addresses in
  ⍝ wblg∆me
  new←log_me wblg∆test_ip wblg∆not_filter old
∇

∇new←wblg∆filter_no_referral old;ref
  ⍝ Function removes entries with a referral field.  Presumably
  ⍝ leaving one record for each visit.
  new←wblg∆test_not_referred wblg∆not_filter old
∇

∇new←suffix wblg∆filter_file_type old
  ⍝ Function removes entries where the file suffix is not in the left
  ⍝ argument.
  new←suffix wblg∆test_suffix wblg∆not_filter old
∇

∇v←wblg∆filter_visitors hits;ip;ix
⍝ Function counts the number of visitors from a list of hits.
  ip←{(4⍴256)⊥⍎¨'.' utl∆split wblg∆ip ⍵}¨hits
  ix←⍋ip
  ip←ip[ix]
  v←(ip≠1⌽ip)/hits[ix]
∇


⍝ ********************************************************************
⍝ Obligatory metadata
⍝ ********************************************************************
∇z←log⍙metadata                         
  z←0 2⍴⍬                                
  z←z⍪'Author'		'Bill Daly'              
  z←z⍪'BugEmail'	'bugs@dalywebandedit∩om'
  z←z⍪'Documentation'	'Function comments'
  z←z⍪'Download' 	''                     
  z←z⍪'License' 	'GPL v3.0'              
  z←z⍪'Portability'	'L3'                   
  z←z⍪'Provides'	''                      
  z←z⍪'Requires'	''                      
  z←z⍪'Version'		'0 1 3'                 
  z←z⍪'Last update'	'2022-05-28'
∇

⍝ ********************************************************************
⍝ Variables
⍝ ********************************************************************

⍝ Administrators' ip addresses
log_me ← '68.81.178.238' '10.30.72.4'
