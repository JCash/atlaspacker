// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

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
void CommandProjectFileNew(HWorker worker, AppState* state);
void CommandProjectFileSave(HWorker worker, AppState* state);
void CommandProjectFileExport(HWorker worker, AppState* state);
void CommandDeleteSelectedImages(HWorker worker, AppState* state);

void CommandAddImageFile(HWorker worker, AppState* state);
void CommandAddImageFolder(HWorker worker, AppState* state);

void CommandFolderOpen(HWorker worker, AppState* state, void (*callback)(void* ctx, const char*), void* ctx);

// Parallel jobs for the worker thread
void CommandRecreateAtlas(HWorker worker, AppState* state);
void CommandLoadImages(HWorker worker, AppState* state);

#endif // ATLASPACKER_TOOL_COMMANDS_H
