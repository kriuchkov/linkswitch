#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>

// Forward declaration
static char* parse_value(const char* input);

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

            char *domain = parse_value(trimmed);
            if (!domain || !domain[0]) { free(domain); continue; }

            new_rule_count++;
            Rule *tmp_rules = realloc(new_rules, sizeof(Rule) * new_rule_count);
            if (!tmp_rules) {
                // Allocation failed: clean up the domain and any partially built rules,
                // leave existing config->rules untouched, and return error.
                int j;
                free(domain);
                // Free any rules that were successfully allocated before this failure.
                for (j = 0; j < new_rule_count - 1; j++) {
                    free(new_rules[j].domain);
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
            new_rules[new_rule_count - 1].match_pattern = NULL;
            new_rules[new_rule_count - 1].domain = domain;
            new_rules[new_rule_count - 1].browser_name = strdup(browser_name);
        }
        fclose(f);
    }
    free(entries);

    if (new_rule_count > 0) {
        size_t old_count = (size_t)config->rule_count;
        Rule *merged = realloc(config->rules, sizeof(Rule) * (old_count + new_rule_count));
        if (!merged) {
            for (int i = 0; i < new_rule_count; i++) {
                free(new_rules[i].domain);
                free(new_rules[i].browser_name);
            }
            free(new_rules);
            return 0;
        }
        config->rules = merged;
        config->rule_count += new_rule_count;
        memmove(config->rules + new_rule_count, config->rules, old_count * sizeof(Rule));
        for (int i = 0; i < new_rule_count; i++) {
            config->rules[i] = new_rules[i];
        }
        free(new_rules);
        return 1;
    }
    free(new_rules);
    return 0;
}

// Parses the value that follows a key on a config line, the way YAML reads
// a scalar: "double-quoted" (\" and \\ are unescaped; any other backslash is
// kept, so a regex such as "github\.com" still works), 'single-quoted' (''
// stands for a quote), or plain, which ends where " #" starts a comment.
// Returns a newly allocated string.
static char* parse_value(const char* input) {
    while (*input == ' ' || *input == '\t') input++;

    char* result = malloc(strlen(input) + 1);
    if (!result) return NULL;
    size_t len = 0;
    const char* p = input;

    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        for (; *p && *p != '\n' && *p != '\r'; p++) {
            if (*p == quote) {
                if (quote == '\'' && p[1] == '\'') {
                    result[len++] = *p++;
                    continue;
                }
                break;
            }
            if (quote == '"' && *p == '\\' && (p[1] == '"' || p[1] == '\\')) p++;
            result[len++] = *p;
        }
    } else {
        for (; *p && *p != '\n' && *p != '\r'; p++) {
            if (*p == '#' && (len == 0 || result[len - 1] == ' ' || result[len - 1] == '\t')) break;
            result[len++] = *p;
        }
        while (len > 0 && (result[len - 1] == ' ' || result[len - 1] == '\t')) len--;
    }
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
            config->default_browser = parse_value(trimmed + 8);
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
            int is_match = strncmp(trimmed, "- match:", 8) == 0;
            int is_domain = strncmp(trimmed, "- domain:", 9) == 0;
            if (is_match || is_domain) {
                if (has_current_rule) {
                    config->rule_count++;
                    config->rules = realloc(config->rules, sizeof(Rule) * config->rule_count);
                    config->rules[config->rule_count - 1] = current_rule;
                }
                memset(&current_rule, 0, sizeof(Rule));
                if (is_match) {
                    current_rule.match_pattern = parse_value(trimmed + 8);
                } else {
                    current_rule.domain = parse_value(trimmed + 9);
                }
                has_current_rule = 1;
            } else if (strncmp(trimmed, "browser:", 8) == 0) {
                if (has_current_rule) {
                    current_rule.browser_name = parse_value(trimmed + 8);
                }
            }
        } else if (in_browsers) {
            if (strncmp(trimmed, "- ", 2) == 0) {
                config->browser_count++;
                config->browsers = realloc(config->browsers, sizeof(char*) * config->browser_count);
                config->browsers[config->browser_count - 1] = parse_value(trimmed + 2);
            }
        } else if (in_profiles) {
            if (strncmp(trimmed, "- name:", 7) == 0) {
                if (has_current_profile) {
                    config->profile_count++;
                    config->profiles = realloc(config->profiles, sizeof(BrowserProfile) * config->profile_count);
                    config->profiles[config->profile_count - 1] = current_profile;
                    memset(&current_profile, 0, sizeof(BrowserProfile));
                }
                current_profile.name = parse_value(trimmed + 7);
                has_current_profile = 1;
            } else if (strncmp(trimmed, "app:", 4) == 0) {
                if (has_current_profile) {
                    current_profile.app_name = parse_value(trimmed + 4);
                }
            } else if (strncmp(trimmed, "args:", 5) == 0) {
                if (has_current_profile) {
                    current_profile.args = parse_value(trimmed + 5);
                }
            } else if (strncmp(trimmed, "cmd:", 4) == 0) {
                if (has_current_profile) {
                    current_profile.custom_cmd = parse_value(trimmed + 4);
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

    // If rules/ directory exists, prepend its rules (directory rules take priority)
    load_rules_from_dir(config, path);

    return config;
}

void free_config(Config *config) {
    if (!config) return;
    if (config->default_browser) free(config->default_browser);
    for (int i = 0; i < config->rule_count; i++) {
        free(config->rules[i].match_pattern);
        free(config->rules[i].domain);
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

// Copies [start, end) into dst, lowercased if asked. Returns 0 if it does not
// fit: a truncated host could end in a domain that the real one does not.
static int copy_span(char *dst, size_t size, const char *start, const char *end, int lower) {
    size_t len = (size_t)(end - start);
    if (len >= size) return 0;
    for (size_t i = 0; i < len; i++) {
        dst[i] = lower ? (char)tolower((unsigned char)start[i]) : start[i];
    }
    dst[len] = '\0';
    return 1;
}

// Splits "host[:port]" or "[ipv6][:port]" into host (lowercased, without a
// trailing dot) and port. Returns 0 if either does not fit.
static int split_host_port(const char *start, const char *end,
                           char *host, size_t host_size, char *port, size_t port_size) {
    const char *host_end = NULL;
    if (*start == '[') {
        host_end = memchr(start, ']', (size_t)(end - start));
        if (host_end) host_end++;
    } else {
        host_end = memchr(start, ':', (size_t)(end - start));
    }
    if (!host_end) host_end = end;

    if (!copy_span(host, host_size, start, host_end, 1)) return 0;
    size_t len = strlen(host);
    if (len > 0 && host[len - 1] == '.') host[len - 1] = '\0';

    const char *port_start = (host_end < end && *host_end == ':') ? host_end + 1 : end;
    return copy_span(port, port_size, port_start, end, 0);
}

static int is_ip_address(const char *host) {
    if (host[0] == '[') return 1;
    for (const char *p = host; *p; p++) {
        if (!isdigit((unsigned char)*p) && *p != '.') return 0;
    }
    return 1;
}

int url_matches_domain(const char *url, const char *rule) {
    if (!url || !rule) return 0;

    // The URL: scheme://[user@]host[:port][/path][?query][#fragment]
    const char *p = url;
    while (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.') p++;
    if (p == url || strncmp(p, "://", 3) != 0) return 0;
    const char *authority = p + 3;
    const char *authority_end = authority + strcspn(authority, "/?#");
    const char *host_start = authority;
    for (const char *s = authority; s < authority_end; s++) {
        if (*s == '@') host_start = s + 1;
    }
    char host[256], port[8];
    if (!split_host_port(host_start, authority_end, host, sizeof(host), port, sizeof(port))) return 0;
    const char *url_path = authority_end;

    // The rule: [scheme://][*.]host[:port][/path]
    const char *r = rule;
    const char *scheme = strstr(r, "://");
    const char *slash = strchr(r, '/');
    if (scheme && (!slash || scheme < slash)) r = scheme + 3;
    if (strncmp(r, "*.", 2) == 0) r += 2;
    else if (*r == '.') r++;
    const char *rule_path = r + strcspn(r, "/");
    char rule_host[256], rule_port[8];
    if (!split_host_port(r, rule_path, rule_host, sizeof(rule_host), rule_port, sizeof(rule_port))) return 0;
    if (!rule_host[0]) return 0;

    size_t host_len = strlen(host), rule_len = strlen(rule_host);
    if (host_len < rule_len || strcmp(host + host_len - rule_len, rule_host) != 0) return 0;
    if (host_len > rule_len) {
        // A subdomain: the rule must cover whole labels, and IP addresses have none.
        if (host[host_len - rule_len - 1] != '.' || is_ip_address(host)) return 0;
    }
    if (rule_port[0] && strcmp(port, rule_port) != 0) return 0;
    if (*rule_path && strncasecmp(url_path, rule_path, strlen(rule_path)) != 0) return 0;
    return 1;
}

char* find_browser_for_url(Config *config, const char *url) {
    if (!config) return NULL;

    for (int i = 0; i < config->rule_count; i++) {
        if (config->rules[i].domain) {
            if (url_matches_domain(url, config->rules[i].domain)) {
                return config->rules[i].browser_name;
            }
            continue;
        }
        if (!config->rules[i].match_pattern) continue;

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
    // default_browser does not replace the picker; main.m falls back on it when
    // the picker's choice can't be opened.
    return NULL;
}

// Wraps s in single quotes for sh.
static char* shell_quote(const char *s) {
    size_t size = 3;
    for (const char *p = s; *p; p++) size += (*p == '\'') ? 4 : 1;
    char *out = malloc(size);
    if (!out) return NULL;
    char *o = out;
    *o++ = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            memcpy(o, "'\\''", 4);
            o += 4;
        } else {
            *o++ = *p;
        }
    }
    *o++ = '\'';
    *o = '\0';
    return out;
}

// Replaces each {url} in a cmd template with a reference to $1, quoted to
// suit where the placeholder stands: bare, inside '...' or inside "...". So
// {url}, '{url}' and "{url}" all expand to the URL as one word, and nothing in
// the URL is parsed as shell syntax.
static char* substitute_url(const char *cmd_template) {
    // The longest replacement, '"${1}"', is 7 characters for the 5 of {url}.
    char *out = malloc(strlen(cmd_template) * 2 + 1);
    if (!out) return NULL;
    char *o = out;
    enum { BARE, SINGLE, DOUBLE } state = BARE;

    for (const char *p = cmd_template; *p; ) {
        if (strncmp(p, "{url}", 5) == 0) {
            const char *ref = state == BARE ? "\"${1}\"" : state == SINGLE ? "'\"${1}\"'" : "${1}";
            size_t len = strlen(ref);
            memcpy(o, ref, len);
            o += len;
            p += 5;
            continue;
        }
        if (state == SINGLE) {
            if (*p == '\'') state = BARE;
        } else if (*p == '\\' && p[1]) {
            *o++ = *p++; // the escaped character below cannot open or close a quote
        } else if (*p == '"') {
            state = (state == DOUBLE) ? BARE : DOUBLE;
        } else if (*p == '\'' && state == BARE) {
            state = SINGLE;
        }
        *o++ = *p++;
    }
    *o = '\0';
    return out;
}

char* build_profile_command(const BrowserProfile *profile) {
    if (!profile) return NULL;
    if (profile->custom_cmd && profile->custom_cmd[0]) {
        return substitute_url(profile->custom_cmd);
    }
    if (!profile->app_name || !profile->app_name[0]) return NULL;

    char *app = shell_quote(profile->app_name);
    if (!app) return NULL;
    // args stay shell syntax, as before: they come from the user's config, not from the link.
    const char *args = profile->args ? profile->args : "";
    size_t size = strlen(app) + strlen(args) + 64;
    char *command = malloc(size);
    if (command) {
        if (args[0]) {
            snprintf(command, size, "/usr/bin/open -n -a %s \"${1}\" --args %s", app, args);
        } else {
            snprintf(command, size, "/usr/bin/open -n -a %s \"${1}\"", app);
        }
    }
    free(app);
    return command;
}
