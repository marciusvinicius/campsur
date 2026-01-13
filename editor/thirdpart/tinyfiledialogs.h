/* tinyfiledialogs.h v3.13 - https://github.com/native-toolkit/tinyfiledialogs
 */
/* This is a minimal version for open/save dialogs only. */
/* For full features, download the latest from the official repo. */

#ifndef TINYFILEDIALOGS_H
#define TINYFILEDIALOGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns selected file path or NULL if cancelled. */
const char *tinyfd_openFileDialog(const char *title,
                                  const char *defaultPathAndFile,
                                  int numOfFilterPatterns,
                                  const char *const *filterPatterns,
                                  const char *singleFilterDescription,
                                  int allowMultipleSelects);

const char *tinyfd_saveFileDialog(const char *title,
                                  const char *defaultPathAndFile,
                                  int numOfFilterPatterns,
                                  const char *const *filterPatterns,
                                  const char *singleFilterDescription);

#ifdef __cplusplus
}
#endif

#endif /* TINYFILEDIALOGS_H */
