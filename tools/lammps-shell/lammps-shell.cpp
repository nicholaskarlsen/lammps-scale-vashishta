// LAMMPS Shell. An improved interactive LAMMPS session with
// command line editing, history, TAB expansion and shell escapes

// Copyright (c) 2020 Axel Kohlmeyer <akohlmey@gmail.com>

// This software is distributed under the GNU General Public License.

#include "library.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#else
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <io.h>
#include <windows.h>
#define chdir(x) _chdir(x)
#define getcwd(buf, len) _getcwd(buf, len)
#define isatty(x) _isatty(x)
#endif

#if !defined(_WIN32)
#include <signal.h>
#endif

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <readline/history.h>
#include <readline/readline.h>

using namespace LAMMPS_NS;

char *omp_threads = nullptr;
const int buflen  = 512;
char buf[buflen];
void *lmp = nullptr;

enum {
    ATOM_STYLE,
    INTEGRATE_STYLE,
    MINIMIZE_STYLE,
    PAIR_STYLE,
    BOND_STYLE,
    ANGLE_STYLE,
    DIHEDRAL_STYLE,
    IMPROPER_STYLE,
    KSPACE_STYLE,
    FIX_STYLE,
    COMPUTE_STYLE,
    REGION_STYLE,
    DUMP_STYLE
};
const char *lmp_style[] = {"atom",    "integrate", "minimize", "pair",   "bond",
                           "angle",   "dihedral",  "improper", "kspace", "fix",
                           "compute", "region",    "dump"};

enum { COMPUTE_ID, DUMP_ID, FIX_ID, MOLECULE_ID, REGION_ID, VARIABLE_ID };
const char *lmp_id[] = {"compute", "dump", "fix", "molecule", "region", "variable"};

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
                         "plugin",
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

static int save_history(std::string range, std::string file)
{
    int from = history_base;
    int to   = from + history_length - 1;

    if (!range.empty()) {
        std::size_t found = range.find_first_of("-");

        if (found == std::string::npos) { // only a single number
            int num = strtol(range.c_str(), NULL, 10);
            if ((num >= from) && (num <= to)) {
                from = to = num;
            } else
                return 1;
        } else {             // range of numbers
            if (found > 0) { // get number before '-'
                int num = strtol(range.substr(0, found).c_str(), NULL, 10);
                if ((num >= from) && (num <= to)) {
                    from = num;
                } else
                    return 1;
            }

            if (range.size() > found + 1) { // get number after '-'
                int num = strtol(range.substr(found + 1).c_str(), NULL, 10);
                if ((num >= from) && (num <= to)) {
                    to = num;
                } else
                    return 1;
            }
        }
        std::ofstream out(file, std::ios::out | std::ios::trunc);
        if (out.fail()) {
            std::cerr << "'" << utils::getsyserror() << "' error when "
                      << "trying to open file '" << file << "' for writing.\n";
            return 0;
        }
        out << "# saved LAMMPS Shell history\n";
        for (int i = from; i <= to; ++i) {
            HIST_ENTRY *item = history_get(i);
            if (item == nullptr) {
                out.close();
                return 1;
            }
            out << item->line << "\n";
        }
        out.close();
    }
    return 0;
}

template <int STYLE> char *style_generator(const char *text, int state)
{
    static int idx, num, len;
    if (!state) {
        idx = 0;
        num = lammps_style_count(lmp, lmp_style[STYLE]);
        len = strlen(text);
    }

    while (idx < num) {
        lammps_style_name(lmp, lmp_style[STYLE], idx, buf, buflen);
        ++idx;
        if ((len == 0) || (strncmp(text, buf, len) == 0)) return dupstring(buf);
    }
    return nullptr;
}

template <int ID> char *id_generator(const char *text, int state)
{
    static int idx, num, len;
    if (!state) {
        idx = 0;
        num = lammps_id_count(lmp, lmp_id[ID]);
        len = strlen(text);
    }

    while (idx < num) {
        lammps_id_name(lmp, lmp_id[ID], idx, buf, buflen);
        ++idx;
        if ((len == 0) || (strncmp(text, buf, len) == 0)) return dupstring(buf);
    }
    return nullptr;
}

template <int ID, char PREFIX> char *ref_generator(const char *text, int state)
{
    char prefix[] = "X_";
    prefix[0]     = PREFIX;

    if (strncmp(text, prefix, 2) == 0) {
        char *id  = id_generator<ID>(text + 2, state);
        char *ref = nullptr;
        if (id) {
            ref = (char *)malloc(strlen(id) + 3);
            if (ref) {
                ref[0] = PREFIX;
                ref[1] = '_';
                ref[2] = 0;
                strcat(ref, id);
            }
            free(id);
        }
        return ref;
    }
    return nullptr;
}

extern "C" {

#if !defined(_WIN32)
static void ctrl_c_handler(int)
#else
static BOOL WINAPI ctrl_c_handler(DWORD event)
#endif
{
#if defined(_WIN32)
    if (event == CTRL_C_EVENT) {
#endif
        if (lmp)
            if (lammps_is_running(lmp)) lammps_force_timeout(lmp);
#if defined(_WIN32)
        return TRUE;
    }
    return FALSE;
#endif
}

static char *cmd_generator(const char *text, int state)
{
    static std::size_t idx, len;
    if (!state) idx = 0;
    len = strlen(text);

    do {
        if ((idx < commands.size()) && ((len == 0) || (commands[idx].substr(0, len) == text)))
            return dupstring(commands[idx++]);
        else
            ++idx;
    } while (idx < commands.size());
    return nullptr;
}

static char *compute_id_generator(const char *text, int state)
{
    return id_generator<COMPUTE_ID>(text, state);
}

static char *compute_ref_generator(const char *text, int state)
{
    return ref_generator<COMPUTE_ID, 'c'>(text, state);
}

static char *dump_id_generator(const char *text, int state)
{
    return id_generator<DUMP_ID>(text, state);
}

static char *fix_id_generator(const char *text, int state)
{
    return id_generator<FIX_ID>(text, state);
}

static char *fix_ref_generator(const char *text, int state)
{
    return ref_generator<FIX_ID, 'f'>(text, state);
}

static char *variable_ref_generator(const char *text, int state)
{
    return ref_generator<VARIABLE_ID, 'v'>(text, state);
}

static char *variable_expand_generator(const char *text, int state)
{
    if (strncmp(text, "${", 2) == 0) {
        char *id  = id_generator<VARIABLE_ID>(text + 2, state);
        char *ref = nullptr;
        if (id) {
            ref = (char *)malloc(strlen(id) + 4);
            if (ref) {
                ref[0] = '$';
                ref[1] = '{';
                ref[2] = 0;
                strcat(ref, id);
                strcat(ref, "}");
            }
            free(id);
        }
        return ref;
    }
    return nullptr;
}

static char *plugin_generator(const char *text, int state)
{
    const char *subcmd[] = {"load", "unload", "list", "clear", NULL};
    const char *sub;
    static std::size_t idx=0, len;
    if (!state) idx = 0;
    len = strlen(text);

    while ((sub = subcmd[idx]) != NULL) {
        ++idx;
        if (strncmp(text,sub,len) == 0)
            return dupstring(sub);
    }
    return nullptr;
}

static char *plugin_style_generator(const char *text, int state)
{
    const char *styles[] = {"pair", "fix", "command", NULL};
    const char *s;
    static std::size_t idx=0, len;
    if (!state) idx = 0;
    len = strlen(text);
    while ((s = styles[idx]) != NULL) {
        ++idx;
        if (strncmp(text,s,len) == 0)
            return dupstring(s);
    }
    return nullptr;
}

static char *plugin_name_generator(const char *text, int state)
{
    auto words = utils::split_words(text);
    if (words.size() < 4) return nullptr;

    static std::size_t idx, len, nmax;
    if (!state) idx = 0;
    len = words[3].size();
    nmax = lammps_plugin_count();

    while (idx < nmax) {
        char style[buflen], name[buflen];
        lammps_plugin_name(idx, style, name, buflen);
        ++idx;
        if (words[2] == style) {
            if (strncmp(name, words[3].c_str(), len) == 0)
                return dupstring(name);
        }
    }
    return nullptr;
}

static char *atom_generator(const char *text, int state)
{
    return style_generator<ATOM_STYLE>(text, state);
}

static char *integrate_generator(const char *text, int state)
{
    return style_generator<INTEGRATE_STYLE>(text, state);
}

static char *minimize_generator(const char *text, int state)
{
    return style_generator<MINIMIZE_STYLE>(text, state);
}

static char *pair_generator(const char *text, int state)
{
    return style_generator<PAIR_STYLE>(text, state);
}

static char *bond_generator(const char *text, int state)
{
    return style_generator<BOND_STYLE>(text, state);
}

static char *angle_generator(const char *text, int state)
{
    return style_generator<ANGLE_STYLE>(text, state);
}

static char *dihedral_generator(const char *text, int state)
{
    return style_generator<DIHEDRAL_STYLE>(text, state);
}

static char *improper_generator(const char *text, int state)
{
    return style_generator<IMPROPER_STYLE>(text, state);
}

static char *kspace_generator(const char *text, int state)
{
    return style_generator<KSPACE_STYLE>(text, state);
}

static char *fix_generator(const char *text, int state)
{
    return style_generator<FIX_STYLE>(text, state);
}

static char *compute_generator(const char *text, int state)
{
    return style_generator<COMPUTE_STYLE>(text, state);
}

static char *region_generator(const char *text, int state)
{
    return style_generator<REGION_STYLE>(text, state);
}

static char *dump_generator(const char *text, int state)
{
    return style_generator<DUMP_STYLE>(text, state);
}

char *group_generator(const char *text, int state)
{
    static int idx, num, len;
    if (!state) {
        idx = 0;
        num = lammps_id_count(lmp, "group");
        len = strlen(text);
    }

    while (idx < num) {
        lammps_id_name(lmp, "group", idx, buf, buflen);
        ++idx;
        if ((len == 0) || (strncmp(text, buf, len) == 0)) return dupstring(buf);
    }
    return nullptr;
}

static char **cmd_completion(const char *text, int start, int)
{
    char **matches = nullptr;

    // avoid segfaults
    if (strlen(text) == 0) return matches;

    if (start == 0) {
        // match command names from the beginning of a line
        matches = rl_completion_matches(text, cmd_generator);
    } else {
        // try to provide context specific matches
        // first split the already completed text into words for position specific expansion
        auto words = utils::split_words(std::string(rl_line_buffer).substr(0, start));

        if (strncmp(text, "c_", 2) == 0) { // expand references to computes or fixes
            matches = rl_completion_matches(text, compute_ref_generator);
        } else if (strncmp(text, "f_", 2) == 0) {
            matches = rl_completion_matches(text, fix_ref_generator);
        } else if (strncmp(text, "v_", 2) == 0) {
            matches = rl_completion_matches(text, variable_ref_generator);
        } else if (strncmp(text, "${", 2) == 0) {
            matches = rl_completion_matches(text, variable_expand_generator);
        } else if (words.size() == 1) { // expand second word
            if (words[0] == "atom_style") {
                matches = rl_completion_matches(text, atom_generator);
            } else if (words[0] == "pair_style") {
                matches = rl_completion_matches(text, pair_generator);
            } else if (words[0] == "bond_style") {
                matches = rl_completion_matches(text, bond_generator);
            } else if (words[0] == "angle_style") {
                matches = rl_completion_matches(text, angle_generator);
            } else if (words[0] == "dihedral_style") {
                matches = rl_completion_matches(text, dihedral_generator);
            } else if (words[0] == "improper_style") {
                matches = rl_completion_matches(text, improper_generator);
            } else if (words[0] == "kspace_style") {
                matches = rl_completion_matches(text, kspace_generator);
            } else if (words[0] == "run_style") {
                matches = rl_completion_matches(text, integrate_generator);
            } else if (words[0] == "min_style") {
                matches = rl_completion_matches(text, minimize_generator);
            } else if (words[0] == "compute_modify") {
                matches = rl_completion_matches(text, compute_id_generator);
            } else if (words[0] == "dump_modify") {
                matches = rl_completion_matches(text, dump_id_generator);
            } else if (words[0] == "fix_modify") {
                matches = rl_completion_matches(text, fix_id_generator);
            } else if (words[0] == "plugin") {
                matches = rl_completion_matches(text, plugin_generator);
            }
        } else if (words.size() == 2) { // expand third word

            // these commands have a group name as 3rd word
            if ((words[0] == "fix")
                || (words[0] == "compute")
                || (words[0] == "dump")) {
                matches = rl_completion_matches(text, group_generator);
            } else if (words[0] == "region") {
                matches = rl_completion_matches(text, region_generator);
            // plugin style is the third word
            } else if ((words[0] == "plugin") && (words[1] == "unload")) {
                matches = rl_completion_matches(text, plugin_style_generator);
            }
        } else if (words.size() == 3) { // expand fourth word

            // style name is the fourth word
            if (words[0] == "fix") {
                matches = rl_completion_matches(text, fix_generator);
            } else if (words[0] == "compute") {
                matches = rl_completion_matches(text, compute_generator);
            } else if (words[0] == "dump") {
                matches = rl_completion_matches(text, dump_generator);
            // plugin name is the fourth word
            } else if ((words[0] == "plugin") && (words[1] == "unload")) {
                matches = rl_completion_matches(rl_line_buffer, plugin_name_generator);
            }
        }
    }

    return matches;
}

} // end of extern "C"

static void init_commands()
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
    commands.push_back("pwd");
    commands.push_back("cd");
    commands.push_back("mem");
    commands.push_back("source");
    commands.push_back("history");
    commands.push_back("clear_history");
    commands.push_back("save_history");

    // set name so there can be specific entries in ~/.inputrc
    rl_readline_name               = "lammps-shell";
    rl_basic_word_break_characters = " \t\n\"\\'`@><=;|&(";

    // attempt completions only if we are connected to a tty or are running tests.
    // otherwise any tabs in redirected input will cause havoc.
    const char *test_mode = getenv("LAMMPS_SHELL_TESTING");
    if (test_mode) std::cout << "*TESTING* using LAMMPS Shell in test mode *TESTING*\n";
    if (isatty(fileno(stdin)) || test_mode) {
        rl_attempted_completion_function = cmd_completion;
    } else {
        rl_bind_key('\t', rl_insert);
    }

    // read saved history, but not in test mode.
    if (!test_mode) read_history(".lammps_history");

#if !defined(_WIN32)
    signal(SIGINT, ctrl_c_handler);
#else
    SetConsoleCtrlHandler(ctrl_c_handler, TRUE);
#endif
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
                 "read at the beginning of the next session of the LAMMPS shell.\n\n"
                 "Additional information is at https://packages.lammps.org/lammps-shell.html\n\n";
    return 0;
}

static int shell_end()
{
    write_history(".lammps_history");
    if (lmp) lammps_close(lmp);
    lammps_mpi_finalize();
    lmp = nullptr;
    return 0;
}

static int shell_cmd(const std::string &cmd)
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
        return shell_end();
    } else if (words[0] == "source") {
        lammps_file(lmp, words[1].c_str());
        free(text);
        return 0;
    } else if ((words[0] == "pwd") || ((words[0] == "cd") && (words.size() == 1))) {
        if (getcwd(buf, buflen)) std::cout << buf << "\n";
        free(text);
        return 0;
    } else if (words[0] == "cd") {
        std::string shellcmd = "shell ";
        shellcmd += text;
        lammps_command(lmp, shellcmd.c_str());
        free(text);
        return 0;
    } else if (words[0] == "mem") {
        double meminfo[3];
        lammps_memory_usage(lmp, meminfo);
        std::cout << "Memory usage.  Current: " << meminfo[0] << " MByte, "
                  << "Maximum : " << meminfo[2] << " MByte\n";
        free(text);
        return 0;
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
    } else if (words[0] == "save_history") {
        free(text);
        if (words.size() == 3) {
            if (save_history(words[1], words[2]) != 0) {
                int from = history_base;
                int to   = from + history_length - 1;
                std::cerr << "Range error: min = " << from << "  max = " << to << "\n";
                return 1;
            }
        } else {
            std::cerr << "Usage: save_history <range> <filename>\n";
            return 1;
        }
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

#if defined(_WIN32)
    // Special hack for Windows: if the current working directory is
    // the "system folder" (because that is where cmd.exe lives)
    // switch to the user's documents directory. Avoid buffer overflow
    // and skip this step if the path is too long for our buffer.
    if (getcwd(buf, buflen)) {
        if ((strstr(buf, "System32") || strstr(buf, "system32"))) {
            char *drive = getenv("HOMEDRIVE");
            char *path  = getenv("HOMEPATH");
            buf[0]      = '\0';
            int len     = strlen("\\Documents");
            if (drive) len += strlen(drive);
            if (path) len += strlen(path);
            if (len < buflen) {
                if (drive) strcat(buf, drive);
                if (path) strcat(buf, path);
                strcat(buf, "\\Documents");
                chdir(buf);
            }
        }
    }
#endif

    lammps_get_os_info(buf, buflen);
    std::cout << "LAMMPS Shell version 1.1  OS: " << buf;

    if (!lammps_config_has_exceptions())
        std::cout << "WARNING: LAMMPS was compiled without exceptions\n"
                     "WARNING: The shell will terminate on errors.\n";

#if defined(_OPENMP)
    int nthreads = omp_get_max_threads();
#else
    int nthreads = 1;
#endif
    // avoid OMP_NUM_THREADS warning and change the default behavior
    // to use the maximum number of threads available since this is
    // not intended to be run with MPI.
    omp_threads = dupstring(std::string("OMP_NUM_THREADS=" + std::to_string(nthreads)));
    putenv(omp_threads);

    // handle the special case where the first argument is not a flag but a file
    // this happens for example when using file type associations on Windows.
    // in this case we save the pointer and remove it from argv.
    // we also get the directory name and switch to that folder
    std::string input_file;
    if ((argc > 1) && (argv[1][0] != '-')) {
        --argc;
        input_file = utils::path_basename(argv[1]);
        chdir(utils::path_dirname(input_file).c_str());
        for (int i = 1; i < argc; ++i)
            argv[i] = argv[i + 1];
    }

    lmp = lammps_open_no_mpi(argc, argv, nullptr);
    if (lmp == nullptr) return 1;

    using_history();
    init_commands();

    // pre-load an input file that was provided on the command line
    if (!input_file.empty()) {
        lammps_file(lmp, input_file.c_str());
    } else {
        for (int i = 0; i < argc; ++i) {
            if ((strcmp(argv[i], "-in") == 0) || (strcmp(argv[i], "-i") == 0)) {
                lammps_file(lmp, argv[i + 1]);
            }
        }
    }

    while (lmp != nullptr) {
        line = readline("LAMMPS Shell> ");
        if (!line) break;
        trimmed = utils::trim(line);
        if (trimmed.size() > 0) {
            shell_cmd(trimmed);
        }
        free(line);
    }

    return shell_end();
}
