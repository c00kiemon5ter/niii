/* niii - ncurses interface to ii similar to iii
 * by Ivan c00kiemon5ter V Kanakarakis   >:3
 * see LICENSE for copyright information
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <curses.h>
#include <locale.h>
#include <event.h>

#define NICKLEN 12
#define ESCSYMB "/CLOSE"

#define USAGE "usage: niii [-v] [-h] <path/to/ircdir/network/channel/>"

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

/* color pair identifiers - up to 8 - see man COLOR_PAIR */
enum { DATETIME, NICK, SEPARATOR, MESG, WINP, };

static WINDOW *winp, *wout;
static FILE *in, *out;
static char ircdir[PATH_MAX];
static int winrows, wincols;
static bool running = true;

/* print the given line formated and colored
 * add the line to the last line of wout
 * older lines should scroll up automatically */
static void printline(const char *date, const char *time, const char *nick, const char *mesg) {
    if (has_colors() == TRUE) wattron(wout, COLOR_PAIR(SEPARATOR));
    wprintw(wout, "\n%s %s ", date, time);
    if (has_colors() == TRUE) wattroff(wout, COLOR_PAIR(SEPARATOR));

    if (has_colors() == TRUE) wattron(wout, COLOR_PAIR(NICK));
    wprintw(wout, "%*.*s ", NICKLEN, NICKLEN, nick);
    if (has_colors() == TRUE) wattroff(wout, COLOR_PAIR(NICK));

    if (has_colors() == TRUE) wattron(wout, COLOR_PAIR(SEPARATOR));
    wprintw(wout, "| ");
    if (has_colors() == TRUE) wattroff(wout, COLOR_PAIR(SEPARATOR));

    if (has_colors() == TRUE) wattron(wout, COLOR_PAIR(MESG));
    wprintw(wout, "%s", mesg);
    if (has_colors() == TRUE) wattroff(wout, COLOR_PAIR(MESG));
}

static void readout(void) {
    char *date, *time, *nick, *mesg;
    char *rawline = NULL;
    size_t len = 0;

    while (getline(&rawline, &len, out) != -1) {
        date = strtok(rawline, " \t");
        time = strtok(NULL, " \t");
        nick = strtok(NULL, " \t");
        mesg = strtok(NULL, "\n");

        if (mesg[0] == '') { /* fix: 'ACTION msg' */
            char mtmp[LINE_MAX];
            mesg += strlen("ACTION ");
            mesg[strlen(mesg) - 1] = '\0';
            snprintf(mtmp, sizeof(mtmp), "%s %s", nick, mesg);
            mesg = mtmp;
            nick = "*\0";
        } else if (nick[0] == '<') { /* nick[strlen(nick) - 1] == '>' */
            ++nick;
            nick[strlen(nick) - 1] = '\0';
        }

        printline(date, time, nick, mesg);
    }

    wrefresh(wout);
    wrefresh(winp); /* leave cursor on input window */
}

static void updatewout(void) {
    rewind(out);
    delwin(wout);
    wout = newwin(winrows - 1, wincols, 0, 0);
    scrollok(wout, true);
    readout();
}

static void updatewinp(void) {
    char *prompt = NULL;
    prompt = strrchr(ircdir, '/') + 1;
    delwin(winp);
    winp = newwin(1, wincols, winrows - 1, 0);
    if (has_colors() == TRUE) wattron(winp, COLOR_PAIR(WINP)|A_BOLD);
    wprintw(winp, "[%s] ", prompt);
    if (has_colors() == TRUE) wattroff(winp, COLOR_PAIR(WINP)|A_BOLD);
    wrefresh(winp);
}

static void updateall(void) {
    updatewout();
    updatewinp();
}

static void redrawall() {
    getmaxyx(stdscr, winrows, wincols);
    updateall();
    redrawwin(wout);
    redrawwin(winp);
}

static void createwins(void) {
    /* start curses mode - do not buffer input */
    initscr();
    /* start color support and set up color pairs */
    if (has_colors() == TRUE) {
        start_color();
        init_pair(DATETIME,  COLOR_CYAN,  COLOR_BLACK);
        init_pair(NICK,      COLOR_GREEN, COLOR_BLACK);
        init_pair(SEPARATOR, COLOR_CYAN,  COLOR_BLACK);
        init_pair(MESG,      COLOR_WHITE, COLOR_BLACK);
        init_pair(WINP,      COLOR_GREEN, COLOR_BLACK);
    }
    /* create the windows and add contents */
    redrawall();
}

static void sendmesg(const char* mesg) {
    if (mesg == NULL) return;
    fprintf(in, "%s\n", mesg);
    fflush(in);
}

static void readinput(void) {
    char *input;
    if ((input = calloc(1, LINE_MAX)) == NULL)
        err(EXIT_FAILURE, "failed to allocate space for input");

    int r = wgetnstr(winp, input, LINE_MAX);
    updatewinp();

    if (r == KEY_RESIZE) redrawall();
    else if (input == NULL) return;
    else if (strlen(input) == 0) redrawall();
    else if (strcmp(input, ESCSYMB) == 0) running = false;
    else sendmesg(input);
}

static void newmesg(int fd, short evtype, void *unused) {
    (void)&fd;     /* warning: unused parameter ‘fd’     [-Wunused-parameter] */
    (void)&evtype; /* warning: unused parameter ‘evtype’ [-Wunused-parameter] */
    (void)&unused; /* warning: unused parameter ‘unused’ [-Wunused-parameter] */
    readout();
}

static void destroywins(void) {
    delwin(winp);
    delwin(wout);
    endwin();
}

int main(int argc, char *argv[]) {
    char infile[PATH_MAX], outfile[PATH_MAX], *abspath;
    int outfd;
	struct stat st;
    struct passwd *pwd = getpwuid(getuid());
    struct event_config *cfg;
    struct event_base *base;
    struct event *watcher;

    if (setlocale(LC_ALL, "") == NULL)
        warn("failed to set locale");

    /* check for switches - only -v and -h are available and must be before any arguments */
    if (argv[1][0] == '-' && argv[1][2] == '\0') switch (argv[1][1]) {
        case 'v': errx(EXIT_SUCCESS, "niii - %s - by c00kiemon5ter", VERSION);
        case 'h': errx(EXIT_SUCCESS, "%s", USAGE);
        default:  errx(EXIT_FAILURE, "%s", USAGE);
    }
    /* check for argument - if none use the user's home dir, else use the given dir */
    switch (argc) {
        case 1: snprintf(ircdir, sizeof(ircdir), "%s/irc", pwd->pw_dir); break;
        case 2: strncpy(ircdir, argv[1], sizeof(ircdir)); break;
        default: errx(EXIT_FAILURE, "%s", USAGE);
    }

    /* check if there is indeed such a directorty and get the absolute path */
	if (stat(ircdir, &st) < 0 || !S_ISDIR(st.st_mode))
		err(EXIT_FAILURE, "failed to find directory: %s", ircdir);
    if ((abspath = realpath(ircdir, NULL)) == NULL)
        err(EXIT_FAILURE, "failed to get absolute path for directory: %s", ircdir);
    strncpy(ircdir, abspath, sizeof(ircdir));
    free(abspath);
    /* create the filepaths and open the files - we need to monitor the out descriptor */
    sprintf(outfile, "%s/out", ircdir);
    sprintf(infile, "%s/in", ircdir);
    if ((in = fopen(infile, "w")) == NULL)
        err(EXIT_FAILURE, "failed to open file: %s", infile);
    if ((outfd = open(outfile, O_RDONLY)) == -1)
        err(EXIT_FAILURE, "failed to open descriptor for file: %s", outfile);
    if ((out = fdopen(outfd, "r")) == NULL)
        err(EXIT_FAILURE, "failed to open file: %s", outfile);

    /* listen for new messages on out file */
    if ((cfg = event_config_new()) == NULL)
        err(EXIT_FAILURE, "failed to create event loop configuration");
    if (event_config_require_features(cfg, EV_FEATURE_FDS) == -1)
        err(EXIT_FAILURE, "failed to configure event loop");
    if ((base = event_base_new_with_config(cfg)) == NULL)
        err(EXIT_FAILURE, "failed to create event base");
    if ((watcher = event_new(base, outfd, EV_READ|EV_PERSIST, newmesg, NULL)) == NULL)
        err(EXIT_FAILURE, "failed to create watcher for file: %s", outfile);
    if (event_add(watcher, NULL) == -1)
        err(EXIT_FAILURE, "failed to add watcher to pending events");
    //if (event_base_loop(base, EVLOOP_NONBLOCK) == -1)
    //    warn("unhandled event loop backend error");
    //event_base_dispatch(base);
    /* start curses - create windows - read history */
    createwins();
    readout();
    /* handle input */
    while (running) readinput();
    /* cleanup */
    event_base_loopbreak(base);
    event_free(watcher);
    event_base_free(base);
    event_config_free(cfg);
    destroywins();
    fclose(in);
    fclose(out);
    close(outfd);
    /* all was good :] */
    return EXIT_SUCCESS;
}

