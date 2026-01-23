// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#ifndef ATLASPACKER_TOOL_COMMANDS_H
#define ATLASPACKER_TOOL_COMMANDS_H

typedef struct Worker* HWorker;

struct AppState;

// Internal
enum CommandResult
{
    RESULT_OK = 0,
    RESULT_FAILED = -1,
};

// Delay jobs for the main thread
void CommandProjectFileOpen(HWorker worker, AppState* state);
void CommandProjectFileSave(HWorker worker, AppState* state);
void CommandProjectFileExport(HWorker worker, AppState* state);

void CommandAddImageFile(HWorker worker, AppState* state);
void CommandAddImageFolder(HWorker worker, AppState* state);

void CommandFolderOpen(HWorker worker, AppState* state, void (*callback)(void* ctx, const char*), void* ctx);

// Parallel jobs for the worker thread
void CommandRecreateAtlas(HWorker worker, AppState* state);
void CommandLoadImages(HWorker worker, AppState* state);

#endif // ATLASPACKER_TOOL_COMMANDS_H
