#ifndef CONFIG_H
#define CONFIG_H

// A rule has either a match_pattern or a domain; the other one is NULL.
typedef struct {
    char *match_pattern; // POSIX extended regex, matched anywhere in the URL
    char *domain;        // host matched with its subdomains, see url_matches_domain
    char *browser_name;
} Rule;

typedef struct {
    char *name;
    char *app_name;
    char *args;
    char *custom_cmd;
} BrowserProfile;

typedef struct {
    char *default_browser;
    Rule *rules;
    int rule_count;
    char **browsers;
    int browser_count;
    BrowserProfile *profiles;
    int profile_count;
} Config;

// Loads config from the given path. Returns NULL on failure.
Config* load_config(const char *path);

// Frees the config structure.
void free_config(Config *config);

// Returns the name of the browser to use.
char* find_browser_for_url(Config *config, const char *url);

// Returns the profile with the given name, or NULL if not found.
BrowserProfile* find_profile(Config *config, const char *name);

// Reports whether the host of url is the rule's host or one of its
// subdomains. The rule may add a port ("localhost:3000") and a path the URL
// must start with ("github.com/my-org").
int url_matches_domain(const char *url, const char *rule);

// Returns a newly allocated sh script that opens a URL in the profile, or
// NULL if the profile has neither cmd nor app. Run it as
// `/bin/sh -c <script> linkswitch <url>`: the script reads the URL from $1,
// so the URL is never parsed as shell syntax.
char* build_profile_command(const BrowserProfile *profile);

#endif
