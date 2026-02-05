/* Minimal tinyfiledialogs.c for open/save dialogs. For full version, see official repo. */
#define _POSIX_C_SOURCE 200809L
#include "tinyfiledialogs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* On Linux, use zenity for dialogs. */
static char result[4096];

const char *tinyfd_openFileDialog(const char *title, const char *defaultPathAndFile,
    int numOfFilterPatterns, const char * const *filterPatterns,
    const char *singleFilterDescription, int allowMultipleSelects)
{
    (void)title; (void)defaultPathAndFile; (void)numOfFilterPatterns; (void)filterPatterns; (void)singleFilterDescription; (void)allowMultipleSelects;
    FILE *fp = popen("zenity --file-selection", "r");
    if (!fp) return NULL;
    if (fgets(result, sizeof(result), fp) == NULL) {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    size_t len = strlen(result);
    if (len > 0 && result[len-1] == '\n') result[len-1] = '\0';
    return result;
}

const char *tinyfd_saveFileDialog(const char *title, const char *defaultPathAndFile,
    int numOfFilterPatterns, const char * const *filterPatterns,
    const char *singleFilterDescription)
{
    (void)title; (void)defaultPathAndFile; (void)numOfFilterPatterns; (void)filterPatterns; (void)singleFilterDescription;
    FILE *fp = popen("zenity --file-selection --save --confirm-overwrite", "r");
    if (!fp) return NULL;
    if (fgets(result, sizeof(result), fp) == NULL) {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    size_t len = strlen(result);
    if (len > 0 && result[len-1] == '\n') result[len-1] = '\0';
    return result;
}
