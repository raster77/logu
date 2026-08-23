# log-utils

A command-line tool for merging and browsing application log files.

It merges log files from the same application into a single,
chronologically-sorted log — matching each entry's timestamp and grouping
continuation lines (e.g. stack traces) with the entry they belong to. It can
also aggregate every log file found in a directory tree, or simply browse a
single, already-merged log file in an interactive terminal viewer. `.gz`
files are decompressed transparently wherever a log file is expected.

## Usage

```
log-utils FILE1 FILE2 [FILE3 ...] [options]
log-utils --view FILE [options]
log-utils --working-dir DIR [options]
```

### Modes

- **Merge files** — pass two or more log file paths as positional arguments.
  Entries from every file are combined and sorted chronologically by
  timestamp; when timestamps are equal, entries keep the relative order of
  the files as given on the command line.
- **`--view FILE`** — browse a single, already-merged log file instead of
  merging. Cannot be combined with the file list. Opens the interactive
  viewer by default; pass `-o` or `--no-interactive` for plain output
  instead.
- **`--working-dir DIR`** — aggregate every log file found inside `DIR`,
  including subdirectories, into one merged log (files are matched
  regardless of extension). Cannot be combined with the file list or
  `--view`. Without `-o`, a non-interactive run defaults to writing the
  result to `DIR/merged.log`; an interactive run instead writes it to a temp
  file that's deleted again on quit, same as a plain merge (see `-o` below).
  As with a plain merge, all entries across every aggregated file are sorted
  chronologically by timestamp; files are read in path order, so entries
  with equal timestamps keep that order. In interactive mode, `DIR` is also
  watched in the background for added, removed, or modified files — see
  [Working directory changes](#working-directory-changes) below.

### Options

| Option | Description |
| --- | --- |
| `FILE1 FILE2 ...` | Two or more log files to merge (positional arguments). |
| `--view FILE` | Browse a single already-merged log file in the interactive viewer instead of merging. |
| `--working-dir DIR` | Aggregate every log file found inside `DIR` (recursively) into one merged log saved in `DIR`. |
| `-o, --output PATH` | Output file. Defaults to stdout for a plain merge, or `DIR/merged.log` for a non-interactive `--working-dir` run. Combine with `--interactive` to save while also browsing the result. Without `-o`, an interactive session (plain merge or `--working-dir`) instead writes to a temp file — created exclusively and readable only by you, and deleted again on quit. |
| `--formats PATH` | JSON file of timestamp formats to recognize (default: `formats.json`), detected once per file. See [Timestamp formats](#timestamp-formats) below. |
| `--dedup` | Remove entries whose content is identical — across every file when merging, or within the file when using `--view`. |
| `-i, --interactive` / `--no-interactive` | Browse the result in the interactive terminal viewer. On by default with `--view`; use `--no-interactive` to force plain output. Add `-o` to also save the result. |
| `-v, --verbose` | Print the timestamp format detected for each file to stderr (or a warning if none matched). Useful for diagnosing a file that isn't being split into entries correctly. |
| `--theme NAME` | Colour theme for the interactive viewer, matched case-insensitively (default: `nord-dark`, or the theme last saved via `/theme` — see below). Available themes: `dark`, `catppuccin`, `catppuccin-frappe`, `catppuccin-macchiato`, `cursor`, `dracula`, `flexoki`, `monokai`, `nord-dark`, `nord-light`, `one-double`, `openCode`, `orng`, `osaka-jade`, `palenight`, `tokyonight`, `vercel`. |
| `-h, --help` | Show usage help. |

### Timestamp formats

A line starts a new entry when it begins with a recognized timestamp;
otherwise it's treated as a continuation of the previous entry (e.g. a stack
trace). Each file's format is detected once and then reused for every line
in that file, rather than re-checked line by line. Detection votes across
the file's first 2000 lines: each line casts a single vote, for the *first*
format in `formats.json` that recognizes it, and the format with the most
votes wins. So one atypical line — a header without milliseconds, say —
can't lock in a format the rest of the file doesn't use.

Note that a line votes only once, for the first format that fits it, not for
every format that could. That matters because formats are often prefixes of
each other: `plain` (`yyyy-MM-dd HH:mm:ss`) matches every line `logback`
(the same plus `.SSS`) matches. Counting all matches would let `plain` tie
or beat `logback` on any Logback file, dropping the fraction from every
timestamp; letting each line vote for the most specific format that fits it
keeps the majority of ordinary lines in charge. The practical consequence is
that **order matters beyond ties** — list more specific or more likely
formats first. Ties also go to whichever format is listed first, which is
what decides files short or uniform enough that only one line votes. If no
format matches any of those first 2000 lines, detection falls back to the
first line that matches anywhere in the file, so logs whose timestamps only
start later still work.

This assumes a file's entries all come from the same application/logger,
consistent with this tool's purpose; if that ever doesn't hold for a given
file (e.g. its format changed partway through), lines after the switch
would stop being recognized as new entries.

A format is `{"name": ..., "pattern": ...}`, where `pattern` uses
SimpleDateFormat-style tokens:

| Token | Meaning |
| --- | --- |
| `yyyy` | 4-digit year |
| `MM` | 2-digit month |
| `MMM` | Month name (case-insensitive), longest match wins: English 3-letter abbreviations (`May`) or the French ones glibc's `%b` produces under `fr_FR` (`janv.`, `févr.`, `mars`, `avr.`, `mai`, `juin`, `juil.`, `août`, `sept.`, `oct.`, `nov.`, `déc.`) |
| `dd` | 2-digit day |
| `HH` | 2-digit hour |
| `mm` | 2-digit minute |
| `ss` | 2-digit second |
| `SSS` | Variable-length fractional-second digits, normalized internally so files logging milliseconds and files logging nanoseconds sort against each other correctly. At most the first nine digits affect the ordering; any further ones are still consumed (so a literal after `SSS` lines up) but ignored |
| `EEE` | Weekday name (case-insensitive, same English/French families as `MMM`); matched and discarded -- it isn't cross-checked against the date, and doesn't affect the sortKey |
| `hh` | 2-digit 12-hour clock hour |
| `a` | AM/PM designator (case-insensitive); combined with `hh` to produce the 24-hour value in the sortKey. An `hh` with no `a` in the same pattern is still reduced mod 12 — `12` reads as hour 0 and `13` as hour 1 — since `hh` means a 12-hour clock either way. Use `HH` for a 24-hour clock |

Any other character is a literal that must match exactly. A pattern must
include month, day, hour (either `HH` or `hh`), minute and second; year, the
fraction, the weekday and the AM/PM designator are all optional. An omitted
year sorts as `0000`, ahead of every dated entry -- fine for a log that only
ever covers one year, but two files spanning different real years and both
using a year-less format will interleave incorrectly, since there's no year
to tell them apart by. The repo's
`formats.json` ships with these formats out of the box, most specific first
within each family so a fraction is captured when present:

```json
[
	{ "name": "logback", "pattern": "yyyy-MM-dd HH:mm:ss.SSS" },
	{ "name": "logback-iso", "pattern": "yyyy-MM-ddTHH:mm:ss,SSS" },
	{ "name": "logback-comma", "pattern": "yyyy-MM-dd HH:mm:ss,SSS" },
	{ "name": "iso8601", "pattern": "yyyy-MM-ddTHH:mm:ss.SSS" },
	{ "name": "iso8601-nofrac", "pattern": "yyyy-MM-ddTHH:mm:ss" },
	{ "name": "apache", "pattern": "dd/MMM/yyyy:HH:mm:ss.SSS" },
	{ "name": "apache-common", "pattern": "dd/MMM/yyyy:HH:mm:ss" },
	{ "name": "nginx", "pattern": "yyyy/MM/dd HH:mm:ss" },
	{ "name": "plain", "pattern": "yyyy-MM-dd HH:mm:ss" },
	{ "name": "vmtoolsd", "pattern": "[MMM dd HH:mm:ss.SSS]" },
	{ "name": "syslog", "pattern": "MMM dd HH:mm:ss" },
	{ "name": "redis", "pattern": "dd MMM yyyy HH:mm:ss.SSS" },
	{ "name": "oracle", "pattern": "dd-MMM-yyyy HH:mm:ss" },
	{ "name": "ctime", "pattern": "EEE MMM dd HH:mm:ss yyyy" },
	{ "name": "http-date", "pattern": "EEE, dd MMM yyyy HH:mm:ss" },
	{ "name": "us-12h", "pattern": "MM/dd/yyyy hh:mm:ss a" }
]
```

| Format | Example | Typical source |
| --- | --- | --- |
| `logback` | `2026-08-06 14:23:01.123` | Logback/Spring Boot default layout |
| `logback-iso` | `2026-08-06T14:23:01,123` | Logback's ISO8601 layout |
| `logback-comma` | `2026-08-06 14:23:01,123` | Python `logging` default / classic log4j |
| `iso8601` | `2026-08-06T14:23:01.123` | JSON-derived/ISO-instant logs; also matches RFC3339Nano lines like Docker/Kubernetes container logs (a trailing `Z` or offset after the fraction is simply left unmatched) |
| `iso8601-nofrac` | `2026-08-06T14:23:01` | ISO 8601/RFC 3339 timestamps with no fraction, e.g. Kubernetes logs (`...01Z`) or an ISO timestamp with a UTC offset instead of milliseconds (`...01+11:00`) — the trailing `Z`/offset is left unmatched, same as `iso8601` |
| `apache` | `20/May/2026:08:17:43.173` | Apache/NCSA log format with a custom fractional-seconds field |
| `apache-common` | `20/May/2026:08:17:43` | Standard Apache/NCSA common/combined log format (no fraction); also matches Apache-style access-log timestamps with a trailing offset (`... +1100`), offset left unmatched |
| `nginx` | `2026/08/06 14:23:01` | Nginx error log; also Go's standard `log` package default |
| `plain` | `2026-08-06 14:23:01` | Generic fallback for space-separated timestamps with no fraction |
| `vmtoolsd` | `[juin 26 21:07:22.467]` | VMware Tools guest log style: bracketed, no year, and often a localized month name |
| `syslog` | `Aug 21 12:34:56` | Classic syslog (`/var/log/messages`) and `systemd`/`journalctl` timestamps: no year, no brackets |
| `redis` | `21 Aug 2026 12:34:56.123` | Redis/HAProxy-style space-separated day-month-name-year layout; like most timestamp-first patterns here, doesn't match if a PID/role prefix precedes it on the line (see the start-of-line note below) |
| `oracle` | `21-AUG-2026 12:34:56` | Oracle's default `NLS_DATE_FORMAT`-style dash-separated date, common in DB app logs and `ORA-` error output |
| `ctime` | `Wed Aug 21 12:34:56 2026` | C `ctime()`/`asctime()` default output -- OpenSSL cert dates, cron, `git log`'s default format, and pre-12c Oracle alert logs |
| `http-date` | `Mon, 21 Aug 2026 12:34:56 GMT` | RFC 1123 / HTTP `Date` header format; the trailing zone name is left unmatched, same as the offset/`Z` in the ISO formats above |
| `us-12h` | `08/21/2026 12:34:56 PM` | US-style `MM/dd/yyyy` with a 12-hour clock and AM/PM designator, as seen in some Windows/IIS and legacy enterprise logs |

Note that matching only looks at the *start* of a line, so formats where
the timestamp isn't the first thing on the line (e.g. Apache/Nginx *access*
logs, which start with the client IP) aren't recognized as-is.

These are also the built-in defaults used when `formats.json` (or whatever
file `--formats` points to) is missing or invalid, so the tool works out of
the box even without the file. Add more entries to recognize other log
formats without recompiling.

## Interactive viewer

The interactive viewer lets you scroll, filter, search, and switch themes
without leaving the terminal. Type a command starting with `/` and press
Enter to run it.

### Commands

| Command | Description |
| --- | --- |
| `/filter <text>` | Filter lines by substring, supporting multiple terms (blank clears). |
| `/regexp <pattern>` | Filter lines by regular expression, case-insensitive, supporting multiple terms (blank clears). |
| `/find <text>` | Jump to and highlight the next match (blank clears; `Ctrl+N`/`Ctrl+P` repeat). |
| `/clear [all\|filter\|find]` | Clear the filter and find highlight (blank or `all`), or just one of them (`filter` or `find`). |
| `/theme [name]` | Choose a colour theme (blank opens the picker); the name is matched case-insensitively. The choice is saved to `~/.logu/theme` and reused as the default on future runs (`--theme` still overrides it for a single run). |
| `/copy` | Copy the focused entry to the clipboard, including its stack trace. |
| `/export <path>` | Write the displayed lines (filtered or not) to a file, overwriting it if it exists. A leading `~` is expanded to your home directory (nothing else does it here — the path never passed through a shell). |
| `/stats` | Show a log-level breakdown (count per `TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`) of the currently visible entries. |
| `/help` | Show the command and keyboard shortcut reference. |
| `/quit` | Exit the viewer. |
| `/exit` | Exit the viewer (alias for `/quit`). |

Pressing Enter on a highlighted suggestion *completes* it rather than running
it, if that command takes an argument: `/theme` + Enter leaves `/theme ` in the
box ready for a name, and a second Enter runs it — which is how you reach the
picker and how `/clear` with no target is submitted. Commands that take no
argument at all (`/help`, `/copy`, `/stats`, `/quit`) run on the first Enter.

Command names are matched case-insensitively, so `/FILTER error` and
`/filter error` do the same thing. Spaces around an argument are ignored, so
`/export  out.log ` writes to `out.log`, and a command that takes no argument
tolerates trailing spaces. Trailing *text* does not make a command: `/help now`
is an unknown command rather than `/help`.

### Filter expressions

Both `/filter` and `/regexp` accept multiple terms combined with operators,
matched case-insensitively:

| Operator | Meaning |
| --- | --- |
| (space) or `&&` | AND — both terms must match. |
| `\|\|` | OR — either side must match. |
| `!` | NOT — negates the single term it prefixes (e.g. `!timeout`). |

There's no parenthesized grouping, so precedence is fixed: `!` binds to a
single term, and AND (implicit or `&&`) binds tighter than `||`. Wrap a
term containing spaces in double quotes to keep it as one term instead of
several AND-ed words, e.g. `"connection refused" && !timeout`.

In `/regexp`, each term is itself a regular expression, so
`error || warning` filters to lines matching either pattern.

### Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| `Up` / `Down` | Scroll one line. |
| `PageUp` / `PageDown` | Scroll one page. |
| `Ctrl+Home` / `Ctrl+End` | Jump to the first / last line. |
| `Ctrl+N` / `Ctrl+P` | Next / previous `/find` match. |
| `Ctrl+Y` | Copy the focused entry to the clipboard, including its stack trace. |
| `Esc` | Close the help or stats panel; otherwise clear the command box. |

Copying goes through two paths for reliability: an OSC 52 terminal escape
sequence (works over SSH with no external tool, in any terminal that
supports OSC 52) and, directly, the desktop clipboard via the
[`clip`](https://github.com/dacap/clip) library (X11/Wayland via XWayland,
Windows, macOS) — covering terminals that don't support OSC 52 at all, such
as VTE-based ones (GNOME Terminal, Terminator).

### Working directory changes

When browsing a `--working-dir` aggregation interactively, the directory is
polled in the background (every couple of seconds) for files added,
removed, or modified since the last merge. On a change, a prompt appears in
place of the log list:

```
Enter/y: reload   Esc/n: not now
```

Accepting re-runs the same merge (and dedup, if `--dedup` was passed) over
the directory's current contents, reloads the viewer with the result, and
rewrites the output file (`DIR/merged.log`, or wherever `-o` points).
Declining dismisses the prompt and keeps browsing the current content;
detection resumes either way, so a later change prompts again.
