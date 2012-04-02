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

#define NICKLEN 12
#define ESCSYMB "/CLOSE"
#define SHOWBAR true

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

/* color pair identifiers - up to 8 - see man COLOR_PAIR */
enum { DATETIME, NICK, SEPARATOR, MESG, WBAR, WINP, };

WINDOW *wout, *wbar, *winp; /* output window, input window, bar window */
FILE *out, *in;
char ircdir[PATH_MAX];
bool running = true;

/* print the given line formated and colored
 * add the line to the last line of wout
 * older lines should scroll up automatically */
void printline(const char *date, const char *time, const char *nick, const char *mesg) {
    wattron(wout, COLOR_PAIR(SEPARATOR));
    wprintw(wout, "\n%s %s ", date, time);
    wattroff(wout, COLOR_PAIR(SEPARATOR));

    wattron(wout, COLOR_PAIR(NICK));
    wprintw(wout, "%*.*s ", NICKLEN, NICKLEN, nick);
    wattroff(wout, COLOR_PAIR(NICK));

    wattron(wout, COLOR_PAIR(SEPARATOR));
    wprintw(wout, "| ");
    wattroff(wout, COLOR_PAIR(SEPARATOR));

    wattron(wout, COLOR_PAIR(MESG));
    wprintw(wout, "%s", mesg);
    wattroff(wout, COLOR_PAIR(MESG));
}

void readout(void) {
    char *date, *time, *nick, *mesg;
    char *rawline = NULL;
    size_t len = 0;

    while (getline(&rawline, &len, out) != -1) {
        date = strtok(rawline, " \t");
        time = strtok(NULL, " \t");
        nick = strtok(NULL, " \t");
        mesg = strtok(NULL, "\n");

        if (nick[0] == '<') { /* nick[strlen(nick) - 1] == '>' */
            ++nick;
            nick[strlen(nick) - 1] = '\0';
        }

        printline(date, time, nick, mesg);
    }

    wrefresh(wout);
    wrefresh(winp); /* leave cursor on input window */
}

void updatewout(void) {
    rewind(out);
    werase(wout);
    readout();
}

void updatewbar(void) {
    char *tok = strtok(ircdir, "/"), *netw = tok, *chan = NULL;
    werase(wbar);
    while ((tok = strtok(NULL, "/")) != NULL) {
        netw = chan;
        chan = tok;
    }
    if (has_colors() == TRUE) wattron(wbar, A_BOLD|COLOR_PAIR(WBAR));
    wprintw(wbar, "[%s] [%s]", netw, chan);
    if (has_colors() == TRUE) wattroff(wbar, A_BOLD|COLOR_PAIR(WBAR));
    wrefresh(wbar);
}

void updatewinp(void) {
    werase(winp);
    if (has_colors() == TRUE) wattron(winp, COLOR_PAIR(WINP));
    wprintw(winp, ">> ");
    if (has_colors() == TRUE) wattroff(winp, COLOR_PAIR(WINP));
    wrefresh(winp);
}

void updateall(void) {
    updatewout();
    if (SHOWBAR) updatewbar();
    updatewinp();
}

void createwins(void) {
    /* start curses mode - do not buffer input */
    initscr();
    cbreak();
    /* start color support and set up color pairs */
    if (has_colors() == TRUE) start_color();
    init_pair(DATETIME,  COLOR_CYAN,  COLOR_BLACK);
    init_pair(NICK,      COLOR_GREEN, COLOR_BLACK);
    init_pair(SEPARATOR, COLOR_CYAN,  COLOR_BLACK);
    init_pair(MESG,      COLOR_WHITE, COLOR_BLACK);
    init_pair(WBAR,      COLOR_GREEN, COLOR_BLACK);
    init_pair(WINP,      COLOR_CYAN,  COLOR_BLACK);
    /* create the windows - wout is scrollable */
    wout = newwin(LINES - (SHOWBAR ? 2:1), COLS, 0, 0);
    scrollok(wout, TRUE);
    if (SHOWBAR) wbar = newwin(1, COLS, LINES - 2, 0);
    winp = newwin(1, COLS, LINES - 1, 0);
    /* add contents */
    updateall();
}

void sendmesg(const char* mesg) {
    if (mesg == NULL) return;
    fprintf(in, "%s\n", mesg);
    fflush(in);
}

void readinput(void) {
    char *input;
    if ((input = malloc(LINE_MAX)) == NULL)
        err(EXIT_FAILURE, "failed to allocate space for input");

    wgetnstr(winp, input, LINE_MAX);
    updatewinp();

    if (input == NULL || strlen(input) == 0) return;
    if (strcmp(input, ESCSYMB) == 0) running = false;
    else sendmesg(input);
}

void openfiles(void) {
    char outfile[PATH_MAX], infile[PATH_MAX], *abspath;

    /* get absolute path */
    if ((abspath = realpath(ircdir, NULL)) == NULL)
        err(EXIT_FAILURE, "could not get absolute path for directory: %s", ircdir);
    strncpy(ircdir, abspath, sizeof(ircdir));
    free(abspath);

    /* open "out" file to read incoming messages */
    sprintf(outfile, "%s/out", ircdir);
    if ((out = fopen(outfile, "r")) == NULL)
        err(EXIT_FAILURE, "failed to open file: %s", outfile);

    /* open "in" file to write outgoing messages */
    sprintf(infile, "%s/in", ircdir);
    if ((in = fopen(infile, "w")) == NULL)
        err(EXIT_FAILURE, "failed to open file: %s", infile);
}

void cleanup(void) {
    /* close all files */
    fclose(out);
    fclose(in);
    /* delete windows - end curses mode */
    delwin(wout);
    if (SHOWBAR) delwin(wbar);
    delwin(winp);
    endwin();
}

int main(int argc, char *argv[]) {
	struct stat st;
    struct passwd *pwd = getpwuid(getuid());

    setlocale(LC_ALL, "");

    /* check for switches - only -v and -h are available and must be before any arguments */
    if (argv[1][0] == '-' && argv[1][2] == '\0') switch (argv[1][1]) {
        case 'v': fputs("niii - " VERSION " by c00kiemon5ter\n", stdout); exit(EXIT_SUCCESS);
        case 'h': fputs("usage: niii [-v] [-h] <path/to/ircdir/network/channel/>\n", stdout); exit(EXIT_SUCCESS);
        default: fputs("usage: niii [-v] [-h] <path/to/ircdir/network/channel/>\n", stderr); exit(EXIT_FAILURE);
    }
    /* check for argument - if none use the user's home dir, else use the given dir */
    switch (argc) {
        case 1: snprintf(ircdir, sizeof(ircdir), "%s/irc", pwd->pw_dir); break;
        case 2: strncpy(ircdir, argv[1], sizeof(ircdir)); break;
        default: fputs("usage: niii [-v] [-h] <path/to/ircdir/network/channel/>\n", stderr); exit(EXIT_FAILURE);
    }
    /* check if there is indeed such a directorty */
	if (stat(ircdir, &st) < 0 || !S_ISDIR(st.st_mode))
		err(EXIT_FAILURE, "not a valid directory: %s", ircdir);

    openfiles();
    createwins();
    readout(); /* read history */
    while (running) readinput();
    cleanup();

    // TODO: set up listeners on "out" and call readout();
    // TODO: handle resize, or refresh - call updateall
    // TODO: what is SIGWINCH

    /* all was good :] */
    return EXIT_SUCCESS;
}

