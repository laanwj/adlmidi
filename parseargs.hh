#ifndef H_PARSEARGS
#define H_PARSEARGS

int ParseArguments(int argc, char **argv);
#ifdef __WIN32__
int ParseCommandLine(char *cmdline, char **argv);
#endif

#endif

