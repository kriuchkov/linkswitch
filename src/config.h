#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char *match_pattern;
    char *browser_name;
} Rule;

typedef struct {
    char *default_browser;
    Rule *rules;
    int rule_count;
    char **browsers;
    int browser_count;
} Config;

// Loads config from the given path. Returns NULL on failure.
Config* load_config(const char *path);

// Frees the config structure.
void free_config(Config *config);

// Returns the name of the browser to use.
// Returns NULL if no rule matches (implying we should show the picker or use default if configured to do so, 
// but for this app, NULL means "show picker" if we want that behavior, or we can return default).
// The user wants: "If rule matches -> open. If not -> show picker".
// So if this returns NULL, we show picker.
char* find_browser_for_url(Config *config, const char *url);

#endif
