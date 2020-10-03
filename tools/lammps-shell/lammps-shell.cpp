// LAMMPS Shell. An improved interactive LAMMPS session with
// command line editing, history, TAB expansion and shell escapes

// Copyright (c) 2020 Axel Kohlmeyer <akohlmey@gmail.com>

#include "library.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#else
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#define isatty(x) _isatty(x)
#endif

#include <readline/history.h>
#include <readline/readline.h>

using namespace LAMMPS_NS;

const int buflen = 512;
char buf[buflen];

std::vector<std::string> commands;

// this list of commands is generated by:
// grep '!strcmp(command,' ../../src/input.cpp | sed -e 's/^.*!strcmp(command,"\(.*\)".*$/"\1",/'

const char *cmdlist[] = {"clear",
                         "echo",
                         "if",
                         "include",
                         "jump",
                         "label",
                         "log",
                         "next",
                         "partition",
                         "print",
                         "python",
                         "quit",
                         "shell",
                         "variable",
                         "angle_coeff",
                         "angle_style",
                         "atom_modify",
                         "atom_style",
                         "bond_coeff",
                         "bond_style",
                         "bond_write",
                         "boundary",
                         "box",
                         "comm_modify",
                         "comm_style",
                         "compute",
                         "compute_modify",
                         "dielectric",
                         "dihedral_coeff",
                         "dihedral_style",
                         "dimension",
                         "dump",
                         "dump_modify",
                         "fix",
                         "fix_modify",
                         "group",
                         "improper_coeff",
                         "improper_style",
                         "kspace_modify",
                         "kspace_style",
                         "lattice",
                         "mass",
                         "min_modify",
                         "min_style",
                         "molecule",
                         "neigh_modify",
                         "neighbor",
                         "newton",
                         "package",
                         "pair_coeff",
                         "pair_modify",
                         "pair_style",
                         "pair_write",
                         "processors",
                         "region",
                         "reset_timestep",
                         "restart",
                         "run_style",
                         "special_bonds",
                         "suffix",
                         "thermo",
                         "thermo_modify",
                         "thermo_style",
                         "timestep",
                         "timer",
                         "uncompute",
                         "undump",
                         "unfix",
                         "units"};

static char *dupstring(const std::string &text)
{
    int len    = text.size() + 1;
    char *copy = (char *)malloc(len);
    strcpy(copy, text.c_str());
    return copy;
}

extern "C" {
static char *cmd_generator(const char *text, int state)
{
    static std::size_t idx;
    if (!state) idx = 0;

    do {
        if (commands[idx].substr(0, strlen(text)) == text)
            return dupstring(commands[idx++]);
        else
            ++idx;
    } while (idx < commands.size());
    return nullptr;
}

static char **cmd_completion(const char *text, int start, int)
{
    char **matches = nullptr;

    if (start == 0) {
        // match command names from the beginning of a line
        matches = rl_completion_matches(text, cmd_generator);
    } else {
        // try to provide context specific matches
        // first split the already completed text
        auto words = utils::split_words(std::string(rl_line_buffer).substr(0, start));

        // these commands have a group id as 3rd word
        if ((words.size() == 2) &&
            ((words[0] == "fix") || (words[0] == "compute") || (words[0] == "dump"))) {
            std::cout << "#words: " << words.size() << "\n";
        }
    }
    return matches;
}
} // end of extern "C"

static void init_commands(void *lmp)
{
    // store internal commands
    int ncmds = sizeof(cmdlist) / sizeof(const char *);
    for (int i = 0; i < ncmds; ++i)
        commands.push_back(cmdlist[i]);

    // store optional commands from command styles
    ncmds = lammps_style_count(lmp, "command");
    for (int i = 0; i < ncmds; ++i) {
        if (lammps_style_name(lmp, "command", i, buf, buflen)) commands.push_back(buf);
    }

    // store LAMMPS shell specific command names
    commands.push_back("help");
    commands.push_back("exit");
    commands.push_back("history");
    commands.push_back("clear_history");

    // set name so there can be specific entries in ~/.inputrc
    rl_readline_name = "lammps-shell";

    // attempt completions only if we have are connected to tty,
    // otherwise any tabs in redirected input will cause havoc.
    if (isatty(fileno(stdin))) {
        rl_attempted_completion_function = cmd_completion;
    } else {
        rl_bind_key('\t', rl_insert);
    }

    // read old history
    read_history(".lammps_history");
}

static int help_cmd()
{
    std::cout << "\nThis is the LAMMPS Shell. An interactive LAMMPS session with command \n"
                 "line editing, context aware command expansion, and history.\n\n"
                 "- Hit the TAB key any time to try to expand the current word\n"
                 "- Issue shell commands by prefixing them with '|' (Example: '|ls -la')\n"
                 "- Use the '!' character for bash-like history epansion. (Example: '!run)\n\n"
                 "A history of the session will be written to the a file '.lammps_history'\n"
                 "in the current working directory and - if present - this file will be\n"
                 "read at the beginning of the next session of the LAMMPS shell.\n\n";
    return 0;
}

static int lammps_end(void *&lmp)
{
    write_history(".lammps_history");
    if (lmp) lammps_close(lmp);
    lmp = nullptr;
    return 0;
}

static int lammps_cmd(void *&lmp, const std::string &cmd)
{
    char *expansion;
    char *text = dupstring(cmd);
    int retval = history_expand(text, &expansion);

    // history expansion error
    if (retval < 0) {
        free(text);
        free(expansion);
        std::cout << "History error: " << utils::getsyserror() << "\n";
        return 1;
    }

    // use expanded or original text and add to history
    if (retval > 0) {
        free(text);
        text = expansion;
    } else
        free(expansion);

    add_history(text);

    // only print, don't execute.
    if (retval == 2) {
        std::cout << text << "\n";
        free(text);
        return 0;
    }

    // check for commands particular to lammps-shell
    auto words = utils::split_words(text);
    if (words[0][0] == '|') {
        int rv = system(text + 1);
        free(text);
        return rv;
    } else if ((words[0] == "help") || (words[0] == "?")) {
        free(text);
        return help_cmd();
    } else if (words[0] == "exit") {
        free(text);
        return lammps_end(lmp);
    } else if (words[0] == "history") {
        free(text);
        HIST_ENTRY **list = history_list();
        for (int i = 0; i < history_length; ++i) {
            std::cout << i + history_base << ": " << list[i]->line << "\n";
        }
        return 0;
    } else if (words[0] == "clear_history") {
        free(text);
        clear_history();
        return 0;
    }

    lammps_command(lmp, text);
    free(text);
    return lammps_has_error(lmp);
}

int main(int argc, char **argv)
{
    char *line;
    std::string trimmed;

    void *lmp = lammps_open_no_mpi(argc, argv, nullptr);
    if (lmp == nullptr) return 1;

    using_history();
    init_commands(lmp);

    // pre-load an input file that was provided on the command line
    for (int i = 0; i < argc; ++i) {
        if ((strcmp(argv[i], "-in") == 0) || (strcmp(argv[i], "-i") == 0)) {
            lammps_file(lmp, argv[i + 1]);
        }
    }

    while (lmp != nullptr) {
        line = readline("LAMMPS Shell> ");
        if (!line) break;
        trimmed = utils::trim(line);
        if (trimmed.size() > 0) {
            lammps_cmd(lmp, trimmed);
        }
        free(line);
    }

    return lammps_end(lmp);
}
