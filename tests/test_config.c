#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#include "src/config.h"

extern char **environ;

// Runs a profile script the way main.m does: sh -c <script> linkswitch <url>.
// Captures stdout into out and returns the exit status.
static int run_script(const char *script, const char *url, char *out, size_t size) {
    const char *out_file = "test_script_out.txt";
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char *argv[] = {"sh", "-c", (char *)script, "linkswitch", (char *)url, NULL};
    pid_t pid;
    int status = -1;
    if (posix_spawn(&pid, "/bin/sh", &actions, NULL, argv, environ) == 0) {
        waitpid(pid, &status, 0);
    }
    posix_spawn_file_actions_destroy(&actions);

    FILE *f = fopen(out_file, "r");
    size_t n = f ? fread(out, 1, size - 1, f) : 0;
    out[n] = '\0';
    if (f) fclose(f);
    remove(out_file);
    return status;
}

// Writes a config file from a string literal.
static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
}

// Mock config file creation for testing
void create_test_config(const char *filename) {
    FILE *f = fopen(filename, "w");
    fprintf(f, "default: Safari\n");
    fprintf(f, "\n");
    fprintf(f, "rules:\n");
    fprintf(f, "  - match: \"zoom.us\"\n");
    fprintf(f, "    browser: \"Zoom\"\n");
    fprintf(f, "  - match: \"github.com|gitlab.com\"\n");
    fprintf(f, "    browser: \"Google Chrome\"\n");
    fprintf(f, "  - match: \"localhost\"\n");
    fprintf(f, "    browser: \"Firefox\"\n");
    fprintf(f, "\n");
    fprintf(f, "browsers:\n");
    fprintf(f, "  - Safari\n");
    fprintf(f, "  - Google Chrome\n");
    fclose(f);
}

int main() {
    const char *test_config_file = "test_config.yaml";
    create_test_config(test_config_file);

    printf("Loading config...\n");
    Config *config = load_config(test_config_file);
    assert(config != NULL);
    
    printf("Testing Default Browser...\n");
    assert(strcmp(config->default_browser, "Safari") == 0);

    printf("Testing Browser List...\n");
    assert(config->browser_count == 2);
    assert(strcmp(config->browsers[0], "Safari") == 0);
    assert(strcmp(config->browsers[1], "Google Chrome") == 0);
    printf("  [PASS] Browser list parsed correctly\n");

    printf("Testing Rule Matching...\n");
    
    // Test Zoom match
    char *browser = find_browser_for_url(config, "https://zoom.us/j/123456");
    assert(browser != NULL);
    assert(strcmp(browser, "Zoom") == 0);
    printf("  [PASS] zoom.us -> Zoom\n");

    // Test GitHub match
    browser = find_browser_for_url(config, "https://github.com/will-stone/browserosaurus");
    assert(browser != NULL);
    assert(strcmp(browser, "Google Chrome") == 0);
    printf("  [PASS] github.com -> Google Chrome\n");

    // Test GitLab match (regex OR)
    browser = find_browser_for_url(config, "https://gitlab.com/user/repo");
    assert(browser != NULL);
    assert(strcmp(browser, "Google Chrome") == 0);
    printf("  [PASS] gitlab.com -> Google Chrome\n");

    // Test Localhost match
    browser = find_browser_for_url(config, "http://localhost:8080");
    assert(browser != NULL);
    assert(strcmp(browser, "Firefox") == 0);
    printf("  [PASS] localhost -> Firefox\n");

    // Test No Match (should return NULL)
    browser = find_browser_for_url(config, "https://www.google.com");
    assert(browser == NULL);
    printf("  [PASS] google.com -> NULL (Show Picker)\n");

    free_config(config);
    remove(test_config_file);

    printf("\nTesting Directory-Based Rules...\n");
    mkdir("test_rules_dir", 0755);
    mkdir("test_rules_dir/rules", 0755);

    FILE *cfg = fopen("test_rules_dir/config.yaml", "w");
    fprintf(cfg, "default: Safari\n\nbrowsers:\n  - Safari\n  - Google Chrome\n  - Firefox\n  - Zoom\n");
    fclose(cfg);

    FILE *sf = fopen("test_rules_dir/rules/safari", "w");
    fprintf(sf, "google.com\n");
    fclose(sf);

    FILE *gf = fopen("test_rules_dir/rules/google-chrome", "w");
    fprintf(gf, "github.com\ngitlab.com\n");
    fclose(gf);

    FILE *ff = fopen("test_rules_dir/rules/firefox", "w");
    fprintf(ff, "localhost\n");
    fclose(ff);

    FILE *zf = fopen("test_rules_dir/rules/zoom", "w");
    fprintf(zf, "zoom.us\n");
    fclose(zf);

    config = load_config("test_rules_dir/config.yaml");
    assert(config != NULL);

    browser = find_browser_for_url(config, "https://www.google.com/search");
    assert(browser != NULL);
    assert(strcmp(browser, "Safari") == 0);
    printf("  [PASS] google.com -> Safari (from rules/safari)\n");

    browser = find_browser_for_url(config, "https://github.com/user/repo");
    assert(browser != NULL);
    assert(strcmp(browser, "Google Chrome") == 0);
    printf("  [PASS] github.com -> Google Chrome (from rules/google-chrome)\n");

    browser = find_browser_for_url(config, "http://localhost:8080");
    assert(browser != NULL);
    assert(strcmp(browser, "Firefox") == 0);
    printf("  [PASS] localhost -> Firefox (from rules/firefox)\n");

    browser = find_browser_for_url(config, "https://zoom.us/j/123");
    assert(browser != NULL);
    assert(strcmp(browser, "Zoom") == 0);
    printf("  [PASS] zoom.us -> Zoom (from rules/zoom)\n");

    browser = find_browser_for_url(config, "https://example.com");
    assert(browser == NULL);
    printf("  [PASS] example.com -> NULL (no rule)\n");

    free_config(config);
    remove("test_rules_dir/rules/safari");
    remove("test_rules_dir/rules/google-chrome");
    remove("test_rules_dir/rules/firefox");
    remove("test_rules_dir/rules/zoom");
    rmdir("test_rules_dir/rules");
    remove("test_rules_dir/config.yaml");
    rmdir("test_rules_dir");

    printf("\nTesting Merged Config + Directory Rules...\n");
    mkdir("test_merge_dir", 0755);
    mkdir("test_merge_dir/rules", 0755);

    cfg = fopen("test_merge_dir/config.yaml", "w");
    fprintf(cfg, "default: Safari\n\n");
    fprintf(cfg, "rules:\n");
    fprintf(cfg, "  - match: \"zoom.us\"\n");
    fprintf(cfg, "    browser: \"Zoom\"\n");
    fprintf(cfg, "  - match: \"github.com\"\n");
    fprintf(cfg, "    browser: \"Google Chrome\"\n");
    fprintf(cfg, "\nbrowsers:\n");
    fprintf(cfg, "  - Safari\n");
    fprintf(cfg, "  - Google Chrome\n");
    fprintf(cfg, "  - Zoom\n");
    fclose(cfg);

    sf = fopen("test_merge_dir/rules/safari", "w");
    fprintf(sf, "google.com\ngithub.com\n");
    fclose(sf);

    config = load_config("test_merge_dir/config.yaml");
    assert(config != NULL);

    browser = find_browser_for_url(config, "https://zoom.us/j/123");
    assert(browser != NULL);
    assert(strcmp(browser, "Zoom") == 0);
    printf("  [PASS] zoom.us -> Zoom (from config)\n");

    browser = find_browser_for_url(config, "https://github.com/user/repo");
    assert(browser != NULL);
    assert(strcmp(browser, "Safari") == 0);
    printf("  [PASS] github.com -> Safari (directory overrides config)\n");

    browser = find_browser_for_url(config, "https://www.google.com/search");
    assert(browser != NULL);
    assert(strcmp(browser, "Safari") == 0);
    printf("  [PASS] google.com -> Safari (from rules/safari)\n");

    free_config(config);
    remove("test_merge_dir/rules/safari");
    rmdir("test_merge_dir/rules");
    remove("test_merge_dir/config.yaml");
    rmdir("test_merge_dir");

    printf("\nTesting Value Parsing...\n");
    write_file("test_values.yaml",
        "profiles:\n"
        "  - name: \"Chrome Work\"\n"
        "    app: \"Google Chrome\"\n"
        "    args: \"--profile-directory='Profile 1'\"  # inner quotes survive\n"
        "  - name: \"Firefox Private\"\n"
        "    cmd: \"/Applications/Firefox.app/Contents/MacOS/firefox --private-window '{url}'\"\n"
        "  - name: Escaped\n"
        "    cmd: \"/Applications/Google\\\\ Chrome.app/x \\\"{url}\\\"\"\n"
        "  - name: 'It''s quoted'\n"
        "    app: Safari\n"
        "  - name: No App\n"
        "    args: --incognito\n"
        "rules:\n"
        "  - match: 'zoom\\.us'\n"
        "    browser: zoom.us # the Zoom app\n");
    config = load_config("test_values.yaml");
    assert(config != NULL);
    assert(config->profile_count == 5);
    assert(strcmp(config->profiles[0].args, "--profile-directory='Profile 1'") == 0);
    printf("  [PASS] closing quote inside a quoted value is kept\n");
    assert(strcmp(config->profiles[1].custom_cmd,
                  "/Applications/Firefox.app/Contents/MacOS/firefox --private-window '{url}'") == 0);
    printf("  [PASS] cmd ending in '{url}' is kept whole\n");
    assert(strcmp(config->profiles[2].custom_cmd, "/Applications/Google\\ Chrome.app/x \"{url}\"") == 0);
    printf("  [PASS] \\\\ and \\\" are unescaped in double quotes\n");
    assert(strcmp(config->profiles[3].name, "It's quoted") == 0);
    printf("  [PASS] '' is a quote in single quotes\n");
    assert(config->rule_count == 1);
    assert(strcmp(config->rules[0].match_pattern, "zoom\\.us") == 0);
    assert(strcmp(config->rules[0].browser_name, "zoom.us") == 0);
    printf("  [PASS] trailing # comment is dropped\n");

    printf("\nTesting Profile Commands...\n");
    char *script = build_profile_command(&config->profiles[0]);
    assert(strcmp(script, "/usr/bin/open -n -a 'Google Chrome' \"${1}\" --args --profile-directory='Profile 1'") == 0);
    printf("  [PASS] app + args command\n");

    // Swap open for printf to see the words sh hands to it.
    const char *hostile = "https://github.com/$(echo INJECTED)'$(echo INJECTED)'\"`echo INJECTED`\"?a=1&b=$HOME;c|d *";
    char out[1024], expected[1024];
    char probe[1024];
    snprintf(probe, sizeof(probe), "printf '[%%s]'%s", script + strlen("/usr/bin/open"));
    free(script);
    run_script(probe, hostile, out, sizeof(out));
    snprintf(expected, sizeof(expected), "[-n][-a][Google Chrome][%s][--args][--profile-directory=Profile 1]", hostile);
    assert(strcmp(out, expected) == 0);
    printf("  [PASS] app profile: the URL reaches open as one untouched word\n");

    const char *templates[][3] = {
        // cmd template, output prefix, output suffix
        {"printf %s {url}", "", ""},
        {"printf %s '{url}'", "", ""},
        {"printf %s \"{url}\"", "", ""},
        {"printf %s \"--app={url}\"", "--app=", ""},
        {"printf %s '<{url}>' \\'", "<", ">'"},
    };
    for (size_t i = 0; i < sizeof(templates) / sizeof(templates[0]); i++) {
        BrowserProfile profile = {.name = "t", .custom_cmd = (char *)templates[i][0]};
        script = build_profile_command(&profile);
        assert(run_script(script, hostile, out, sizeof(out)) == 0);
        snprintf(expected, sizeof(expected), "%s%s%s", templates[i][1], hostile, templates[i][2]);
        assert(strcmp(out, expected) == 0);
        printf("  [PASS] %s -> %s\n", templates[i][0], script);
        free(script);
    }

    assert(build_profile_command(&config->profiles[4]) == NULL);
    printf("  [PASS] profile with neither app nor cmd -> NULL\n");
    free_config(config);
    remove("test_values.yaml");

    printf("\nTesting Domain Matching...\n");
    struct { const char *rule, *url; int match; } cases[] = {
        {"github.com", "https://github.com/user/repo", 1},
        {"github.com", "https://api.github.com/x", 1},
        {"github.com", "https://GitHub.COM./", 1},
        {"github.com", "https://user:pass@github.com:443/x", 1},
        {"github.com", "https://evil.example/?next=github.com", 0},
        {"github.com", "https://evil.example/github.com", 0},
        {"github.com", "https://github.com@evil.example/", 0},
        {"github.com", "https://github.com.evil.example/", 0},
        {"github.com", "https://notgithub.com/", 0},
        {"github.com", "mailto:me@github.com", 0},
        {"github.com", "file:///Users/me/github.com.html", 0},
        {"zoom.us", "https://zoomXus.example/", 0},
        {"*.zoom.us", "https://us02web.zoom.us/j/1", 1},
        {"https://gitlab.com", "https://gitlab.com/a", 1},
        {"localhost", "http://localhost:8080/", 1},
        {"localhost:3000", "http://localhost:3000/app", 1},
        {"localhost:3000", "http://localhost:8080/", 0},
        {"127.0.0.1", "http://127.0.0.1:8000/", 1},
        {"0.0.1", "http://127.0.0.1/", 0},
        {"[::1]", "http://[::1]:8080/", 1},
        {"gitlab.com/work", "https://gitlab.com/work/repo", 1},
        {"gitlab.com/work", "https://gitlab.com/other/work", 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (url_matches_domain(cases[i].url, cases[i].rule) != cases[i].match) {
            printf("  [FAIL] %s vs %s\n", cases[i].rule, cases[i].url);
            assert(0);
        }
    }
    // A host too long for the buffer must not be cut down to one that ends in
    // the rule: here its first 255 characters end in "github.com".
    char long_url[600] = "https://";
    for (int i = 0; i < 24; i++) strcat(long_url, "aaaaaaaaa.");
    strcat(long_url, "aaaa.github.com.evil.example/");
    assert(url_matches_domain(long_url, "github.com") == 0);
    printf("  [PASS] %zu host, subdomain, port, path and look-alike cases\n", sizeof(cases) / sizeof(cases[0]) + 1);

    mkdir("test_domain_dir", 0755);
    mkdir("test_domain_dir/rules", 0755);
    write_file("test_domain_dir/config.yaml",
        "rules:\n"
        "  - domain: zoom.us\n"
        "    browser: Zoom\n"
        "  - match: \"example\\\\.org\"\n"
        "    browser: Firefox\n"
        "browsers:\n"
        "  - Google Chrome\n");
    write_file("test_domain_dir/rules/google-chrome", "github.com # work\n");
    config = load_config("test_domain_dir/config.yaml");
    assert(config != NULL);
    browser = find_browser_for_url(config, "https://us02web.zoom.us/j/1");
    assert(browser && strcmp(browser, "Zoom") == 0);
    printf("  [PASS] config 'domain:' rule matches a subdomain\n");
    browser = find_browser_for_url(config, "https://example.org/");
    assert(browser && strcmp(browser, "Firefox") == 0);
    printf("  [PASS] config 'match:' regex rule still works next to it\n");
    browser = find_browser_for_url(config, "https://github.com/org/repo");
    assert(browser && strcmp(browser, "Google Chrome") == 0);
    browser = find_browser_for_url(config, "https://evil.example/?next=github.com");
    assert(browser == NULL);
    printf("  [PASS] directory rule matches the host, not the query string\n");
    free_config(config);
    remove("test_domain_dir/rules/google-chrome");
    rmdir("test_domain_dir/rules");
    remove("test_domain_dir/config.yaml");
    rmdir("test_domain_dir");

    printf("\nAll tests passed!\n");
    return 0;
}
