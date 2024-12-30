// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

typedef struct Worker* HWorker;

struct AppState;

// Delay jobs for the main thread
void CommandProjectFileOpen(HWorker worker, AppState* state);
void CommandProjectFileSave(HWorker worker, AppState* state);
void CommandProjectFileExport(HWorker worker, AppState* state);

void CommandAddImageFile(HWorker worker, AppState* state);
void CommandAddImageFolder(HWorker worker, AppState* state);


// Parallel jobs for the worker thread
void CommandRecreateAtlas(HWorker worker, AppState* state);
void CommandLoadImages(HWorker worker, AppState* state);
