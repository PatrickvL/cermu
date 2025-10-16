#pragma once
#ifdef _WIN32
#ifndef GETOPT_H_INCLUDED
#define GETOPT_H_INCLUDED

// Simple getopt implementation for Windows compatibility
// This is a minimal implementation for basic argument parsing

extern char *optarg;
extern int optind, opterr, optopt;

// Long option constants
#define no_argument       0
#define required_argument 1
#define optional_argument 2

// Long option structure
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

int getopt(int argc, char * const argv[], const char *optstring);
int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex);

#endif // GETOPT_H_INCLUDED
#endif // _WIN32