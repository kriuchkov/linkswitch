#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

// Forward declaration
static char* clean_string(const char* input);

// Convert browser name to slug: lowercase, spaces -> hyphens
static void slugify(const char *name, char *out, size_t out_size) {
    size_t j = 0;
    for (const char *p = name; *p && j < out_size - 1; p++) {
        if (*p == ' ') {
            if (j > 0 && out[j - 1] != '-') out[j++] = '-';
        } else if (isalnum((unsigned char)*p) || *p == '-') {
            out[j++] = (char)tolower((unsigned char)*p);
        }
    }
    out[j] = '\0';
}

// Check if slug matches a browser or profile name
static const char* name_for_slug(Config *config, const char *slug) {
    char buf[256];
    for (int i = 0; i < config->browser_count; i++) {
        slugify(config->browsers[i], buf, sizeof(buf));
        if (strcmp(buf, slug) == 0) return config->browsers[i];
    }
    for (int i = 0; i < config->profile_count; i++) {
        if (!config->profiles[i].name) continue;
        slugify(config->profiles[i].name, buf, sizeof(buf));
        if (strcmp(buf, slug) == 0) return config->profiles[i].name;
    }
    return NULL;
}

// Escape regex special chars for literal substring matching
static char* domain_to_regex(const char *domain) {
    size_t len = strlen(domain);
    char *out = malloc(len * 3 + 8);  // worst case: every char escaped + ".*" wrapper
    if (!out) return NULL;
    char *p = out;
    *p++ = '.'; *p++ = '*';
    for (const char *s = domain; *s; s++) {
        if (*s == '.' || *s == '[' || *s == ']' || *s == '(' || *s == ')' ||
            *s == '{' || *s == '}' || *s == '*' || *s == '+' || *s == '?' ||
            *s == '^' || *s == '$' || *s == '|' || *s == '\\') {
            *p++ = '\\';
        }
        *p++ = *s;
    }
    *p++ = '.'; *p++ = '*'; *p = '\0';
    return out;
}

// Compare dirent names for qsort
static int dirent_cmp(const void *a, const void *b) {
    return strcmp((*(const struct dirent**)a)->d_name, (*(const struct dirent**)b)->d_name);
}

// Load rules from rules/ directory. Returns 1 if any rules loaded, 0 otherwise.
static int load_rules_from_dir(Config *config, const char *config_path) {
    char rules_dir[1024];
    const char *last_slash = strrchr(config_path, '/');
    if (last_slash) {
        size_t prefix_len = (size_t)(last_slash - config_path);
        if (prefix_len >= sizeof(rules_dir) - 8) return 0;
        memcpy(rules_dir, config_path, prefix_len);
        rules_dir[prefix_len] = '\0';
        strcat(rules_dir, "/rules");
    } else {
        strcpy(rules_dir, "rules");
    }

    DIR *d = opendir(rules_dir);
    if (!d) return 0;

    struct dirent **entries = NULL;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN) continue;
        struct dirent *copy = malloc(sizeof(struct dirent));
        if (!copy) continue;
        memcpy(copy, e, sizeof(struct dirent));
        // Reallocate entries array to hold the new entry
        struct dirent **tmp = realloc(entries, sizeof(struct dirent*) * (n + 1));
        if (!tmp) { free(copy); continue; }
        entries = tmp;
        entries[n++] = copy;
    }
    closedir(d);

    if (n == 0) {
        if (entries) free(entries);
        return 0;
    }

    qsort(entries, n, sizeof(struct dirent*), dirent_cmp);

    Rule *new_rules = NULL;
    int new_rule_count = 0;

    for (int i = 0; i < n; i++) {
        const char *browser_name = name_for_slug(config, entries[i]->d_name);
        if (!browser_name) {
            free(entries[i]);
            continue;
        }

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", rules_dir, entries[i]->d_name);
        FILE *f = fopen(filepath, "r");
        free(entries[i]);
        if (!f) continue;

        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            if (*trimmed == '#' || *trimmed == '\n' || *trimmed == '\0') continue;

            char *domain = clean_string(trimmed);
            if (!domain || !domain[0]) { free(domain); continue; }

            char *pattern = domain_to_regex(domain);
            free(domain);
            if (!pattern) continue;

            new_rule_count++;
            Rule *tmp_rules = realloc(new_rules, sizeof(Rule) * new_rule_count);
            if (!tmp_rules) {
                // Allocation failed: clean up pattern and any partially built rules,
                // leave existing config->rules untouched, and return error.
                int j;
                free(pattern);
                // Free any rules that were successfully allocated before this failure.
                for (j = 0; j < new_rule_count - 1; j++) {
                    free(new_rules[j].match_pattern);
                    free(new_rules[j].browser_name);
                }
                free(new_rules);
                // Close current file and free remaining directory entries.
                fclose(f);
                for (j = i + 1; j < n; j++) {
                    free(entries[j]);
                }
                free(entries);
                return 0;
            }
            new_rules = tmp_rules;
            new_rules[new_rule_count - 1].match_pattern = pattern;
            new_rules[new_rule_count - 1].browser_name = strdup(browser_name);
        }
        fclose(f);
    }
    free(entries);

    if (new_rule_count > 0) {
        for (int i = 0; i < config->rule_count; i++) {
            free(config->rules[i].match_pattern);
            free(config->rules[i].browser_name);
        }
        free(config->rules);
        config->rules = new_rules;
        config->rule_count = new_rule_count;
        return 1;
    }
    free(new_rules);
    return 0;
}

// Helper to trim whitespace and quotes
static char* clean_string(const char* input) {
    const char* start = input;
    while (*start == ' ' || *start == '\t' || *start == '"' || *start == '\'') start++;
    
    const char* end = input + strlen(input) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '"' || *end == '\'' || *end == '\n' || *end == '\r')) end--;
    
    size_t len = end - start + 1;
    char* result = malloc(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';
    return result;
}

Config* load_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Config *config = malloc(sizeof(Config));
    config->default_browser = NULL;
    config->rules = NULL;
    config->rule_count = 0;
    config->browsers = NULL;
    config->browser_count = 0;
    config->profiles = NULL;
    config->profile_count = 0;

    char line[1024];
    int in_rules = 0;
    int in_browsers = 0;
    int in_profiles = 0;
    Rule current_rule = {0};
    int has_current_rule = 0;
    BrowserProfile current_profile = {0};
    int has_current_profile = 0;

    while (fgets(line, sizeof(line), f)) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '#' || *trimmed == '\n' || *trimmed == '\0') continue;

        if (strncmp(trimmed, "default:", 8) == 0) {
            config->default_browser = clean_string(trimmed + 8);
        } else if (strncmp(trimmed, "rules:", 6) == 0) {
            in_rules = 1;
            in_browsers = 0;
            in_profiles = 0;
        } else if (strncmp(trimmed, "browsers:", 9) == 0) {
            in_browsers = 1;
            in_rules = 0;
            in_profiles = 0;
        } else if (strncmp(trimmed, "profiles:", 9) == 0) {
            in_profiles = 1;
            in_rules = 0;
            in_browsers = 0;
        } else if (in_rules) {
            if (strncmp(trimmed, "- match:", 8) == 0) {
                if (has_current_rule) {
                    config->rule_count++;
                    config->rules = realloc(config->rules, sizeof(Rule) * config->rule_count);
                    config->rules[config->rule_count - 1] = current_rule;
                }
                current_rule.match_pattern = clean_string(trimmed + 8);
                current_rule.browser_name = NULL;
                has_current_rule = 1;
            } else if (strncmp(trimmed, "browser:", 8) == 0) {
                if (has_current_rule) {
                    current_rule.browser_name = clean_string(trimmed + 8);
                }
            }
        } else if (in_browsers) {
            if (strncmp(trimmed, "- ", 2) == 0) {
                config->browser_count++;
                config->browsers = realloc(config->browsers, sizeof(char*) * config->browser_count);
                config->browsers[config->browser_count - 1] = clean_string(trimmed + 2);
            }
        } else if (in_profiles) {
            if (strncmp(trimmed, "- name:", 7) == 0) {
                if (has_current_profile) {
                    config->profile_count++;
                    config->profiles = realloc(config->profiles, sizeof(BrowserProfile) * config->profile_count);
                    config->profiles[config->profile_count - 1] = current_profile;
                    memset(&current_profile, 0, sizeof(BrowserProfile));
                }
                current_profile.name = clean_string(trimmed + 7);
                has_current_profile = 1;
            } else if (strncmp(trimmed, "app:", 4) == 0) {
                if (has_current_profile) {
                    current_profile.app_name = clean_string(trimmed + 4);
                }
            } else if (strncmp(trimmed, "args:", 5) == 0) {
                if (has_current_profile) {
                    current_profile.args = clean_string(trimmed + 5);
                }
            } else if (strncmp(trimmed, "cmd:", 4) == 0) {
                if (has_current_profile) {
                    current_profile.custom_cmd = clean_string(trimmed + 4);
                }
            }
        }
    }
    
    // Add the last rule
    if (has_current_rule && current_rule.browser_name) {
        config->rule_count++;
        config->rules = realloc(config->rules, sizeof(Rule) * config->rule_count);
        config->rules[config->rule_count - 1] = current_rule;
    }

    // Add the last profile
    if (has_current_profile && current_profile.name) {
        config->profile_count++;
        config->profiles = realloc(config->profiles, sizeof(BrowserProfile) * config->profile_count);
        config->profiles[config->profile_count - 1] = current_profile;
    }

    fclose(f);

    // If rules/ directory exists, load rules from there (replaces yaml rules)
    load_rules_from_dir(config, path);

    return config;
}

void free_config(Config *config) {
    if (!config) return;
    if (config->default_browser) free(config->default_browser);
    for (int i = 0; i < config->rule_count; i++) {
        free(config->rules[i].match_pattern);
        free(config->rules[i].browser_name);
    }
    free(config->rules);
    for (int i = 0; i < config->browser_count; i++) {
        free(config->browsers[i]);
    }
    free(config->browsers);
    for (int i = 0; i < config->profile_count; i++) {
        free(config->profiles[i].name);
        free(config->profiles[i].app_name);
        free(config->profiles[i].args);
        free(config->profiles[i].custom_cmd);
    }
    free(config->profiles);
    free(config);
}

BrowserProfile* find_profile(Config *config, const char *name) {
    if (!config || !name) return NULL;
    for (int i = 0; i < config->profile_count; i++) {
        if (strcmp(config->profiles[i].name, name) == 0) {
            return &config->profiles[i];
        }
    }
    return NULL;
}

char* find_browser_for_url(Config *config, const char *url) {
    if (!config) return NULL;

    for (int i = 0; i < config->rule_count; i++) {
        regex_t regex;
        if (regcomp(&regex, config->rules[i].match_pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
            int match = regexec(&regex, url, 0, NULL, 0);
            regfree(&regex);
            if (match == 0) {
                return config->rules[i].browser_name;
            }
        }
    }
    
    // If no rule matches, return NULL to indicate we should show the picker.
    // (Or return default_browser if we wanted to fallback automatically, but the user wants a picker)
    return NULL; 
}
