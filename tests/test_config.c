#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
    
    printf("\nAll tests passed!\n");
    return 0;
}
