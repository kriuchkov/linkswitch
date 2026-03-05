#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include "src/config.h"

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
    
    printf("\nAll tests passed!\n");
    return 0;
}
