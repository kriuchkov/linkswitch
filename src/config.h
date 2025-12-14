#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char *match_pattern;
    char *browser_name;
} Rule;

typedef struct {
    char *name;
    char *app_name;
    char *args;
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

#endif
