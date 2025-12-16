#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

// Helper to trim whitespace and quotes
char* clean_string(const char* input) {
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
