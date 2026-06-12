
// check that the commands in Command.def are ordered in such a way that no
// command is preceeded by its prefix. E.g. )LIBS shall come before )LIB.

#include <iostream>
#include <cstring>

/// the commands to check
const char * commands[] =
{
#define cmd_def(cmd, ...) cmd,
#include "../src/Command.def"
   0
};

//────────────────────────────────────────────────────────────────────────────
int
main(int, char *[])
{
int errors = 0;

   for (const char ** prefix = commands;  *prefix;  ++prefix)
   for (const char ** command = prefix + 1; *command; ++command)
       {
         if (strncasecmp(*command, *prefix, strlen(*prefix)) == 0 &&
             strcmp(*command, "        ") != 0)   // not ]USERCMD conmtinuation
            {
              std::cerr << "*** Error in src/Command.def: command '" << *prefix
                        << "'\n                       is a prefix of '"
                        << *command << "'\n\n";
              ++errors;
            }
       }

   return errors;
}
//────────────────────────────────────────────────────────────────────────────
