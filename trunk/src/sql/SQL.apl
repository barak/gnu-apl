#!apl -f

⍝
⍝ WARNING: this workspace is obsolete. It uses 'lib_sql' ⎕FX 'SQL' (aka.
⍝ native functions) which was the method of choice before ⎕SQL was
⍝ introduced in GNU APL. These days you should use the system function ⎕SQL
⍝ (a lower level interface to SQL servers) and/or workspace Simple_SQL
⍝ (higher level interface to SQL servers) instead.
⍝
⍝ Sooner or later this workspace will be removed.
⍝

⍝  This file contains helper definititions to make the SQL API more
⍝  easy to use. Example use:
⍝
⍝  db ← 'sqlite' SQL∆Connect '/path/to/db/file'
⍝
⍝    Connects to the database of type X with connect arguments Y.
⍝    This call returns the ID of the connection.
⍝
⍝  results ← 'select * from foo where a > ?' SQL∆Select[db] 100
⍝
⍝    Executes the query given in X with the bind parameters in Y.
⍝
⍝  SQL∆Disconnect db
⍝
⍝    Disconnects from the database connection given as X.

∇Z←type SQL∆Connect arg
⍝⍝ Connect to database of type L using connection arguments R.
⍝⍝
⍝⍝ L must be a string indicating the database type. Current supported
⍝⍝ values are 'postgresql' and 'sqlite'.
⍝⍝
⍝⍝ R is the connection parameters which depends on the type of
⍝⍝ database:
⍝⍝
⍝⍝   - For type≡'sqlite': the argument is string pointing to the
⍝⍝     database file.
⍝⍝
⍝⍝   - For type≡'postgresql', the argument is a standard connect
⍝⍝     string as described in the PostgreSQL documentation.
⍝⍝
⍝⍝ This function returns a database handle that should be used when
⍝⍝ using other SQL functions. This value should be seen as an opaque
⍝⍝ handle. It is, however, guaranteed that the handle is a scalar
⍝⍝ value.
  Z←type SQL[1] arg
∇

∇Z←SQL∆Disconnect db
⍝⍝ Disconnect from database R.
⍝⍝
⍝⍝ R is the database handle that should be disconnected. After this
⍝⍝ function has been called, no further operations are to be performed
⍝⍝ on this handle. Future calls to SQL∆Connect may reuse previously
⍝⍝ disconnected handles.
  Z←SQL[2] db
∇

∇Z←statement SQL∆Select[db] args
⍝⍝ Execute a select statement and return the result table.
⍝⍝
⍝⍝ The axis parameter indicates the database handle.
⍝⍝
⍝⍝ L is a select statement to be executed. Positional parameters can
⍝⍝ be supplied by specifying a question mark "?" in the statemement.
⍝⍝
⍝⍝ R is an array containing the values for the positional parameters.
⍝⍝ If the array is of rank 2, the statement will be executed multiple
⍝⍝ times with each row being the values for each call.
⍝⍝
⍝⍝ The return value is a rank-2 array representing the result of the
⍝⍝ select statement. Null values are returned as ⍬ and empty strings
⍝⍝ are returned as ''.
  Z←statement SQL[3,db] args
∇

∇Z←statement SQL∆Exec[db] args
⍝⍝ Execute an SQL statement that does not return a result.
⍝⍝
⍝⍝ This function is identical to SQL∆Select with the exception that it
⍝⍝ is used on statements which do not return a result table.
  Z←statement SQL[4,db] args
∇

∇Z←SQL∆Begin db
⍝⍝ Begin a transaction.
  Z←SQL[5] db
∇

∇Z←SQL∆Commit db
⍝⍝ Commit a transaction.
  Z←SQL[6] db
∇

∇Z←SQL∆Rollback db
⍝⍝ Rolls back the current transaction.
  Z←SQL[7] db
∇

∇Z←SQL∆Tables db
⍝⍝ Return an array containing the name of all tables.
  Z←SQL[8] db
∇

∇Z←db SQL∆Columns table
⍝⍝ Return an array containing information about the columns in the
⍝⍝ given table. Currently, the column layout is as follows:
⍝⍝
⍝⍝   Name
⍝⍝   Type
⍝⍝
⍝⍝ More columns containing extra information may be added in a future
⍝⍝ release.
  Z←db SQL[9] table
∇

∇Z←db (F SQL∆WithTransaction) R;result
⍝⍝ Call function F inside a transaction. F will be called with
⍝⍝ argument R. If an error occurs while F runs, the transaction will
⍝⍝ be rolled back.
  SQL∆Begin db

  →(0≠⎕NC 'X')/dyadic
  result ← '→rollback' ⎕EA 'F Y'
  →commit

dyadic:
  result ← '→rollback' ⎕EA 'X F Y'

commit:
  SQL∆Commit db
  Z ← result
  →end

rollback:
  SQL∆Rollback db
  ⎕ES "Transaction rolled back"
end:
∇

⍝
⍝  Metadata for library
⍝

∇Z←SQL⍙metadata
  Z ← ,[0.5] 'Author' 'Elias Mårtenson'
  Z ← Z,[1] 'BugEmail' 'bug-apl@gnu.org'
  Z ← Z,[1] 'Documentation' ''
  Z ← Z,[1] 'Download' 'https://github.com/lokedhs/apl-sqlite'
  Z ← Z,[1] 'License' 'LGPL'
  Z ← Z,[1] 'Portability' 'L3'
  Z ← Z,[1] 'Provides' 'SQL'
  Z ← Z,[1] 'Requires' ''
  Z ← Z,[1] 'Version' '1.0'
∇

⍝
⍝  Load the native library
⍝

∇sql⍙load_library;result
  →(0≠⎕NC 'SQL')/skip
  result ← 'lib_sql' ⎕FX 'SQL'
  →('SQL'≡result)/skip
  ⎕ES 'Error loading native library'
skip:
∇

sql⍙load_library
)erase sql⍙load_library

⎕←'SQL lib loaded'
