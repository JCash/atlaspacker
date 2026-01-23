// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#ifndef ATLASPACKER_EXPORTER_H
#define ATLASPACKER_EXPORTER_H

#include <atlaspacker/project.h>

// Gets the default options, with edit types and default values
apOptionValue* apExportGetDefaultOptions(apProject* project, const char* exporter_path);

// Allows the exporter to update the options. Return 0 if unsuccessul
int apExportUpdateOptions(apProject* project, const char* exporter_path);

int apExportProject(apProject* project, const char* exporter_path, const char* project_path);

#endif // ATLASPACKER_EXPORTER_H
