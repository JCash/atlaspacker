// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include <stdint.h>

enum TreeNodeType
{
    TN_TYPE_FOLDER = 0,
    TN_TYPE_IMAGE = 1,
};

struct TreeNode
{
    struct TreeNode*  sibling;
    struct TreeNode*  child; // The first child
    TreeNodeType      type; // 0: folder, 1; file

    //void*             data; // type==0: folder name, type==1: Image
    const char*       path;     // folder or image path
    uint64_t          path_hash;
    uint8_t           selected:1;
    uint8_t           readonly:1;
    uint8_t           :6;
};


TreeNode*   TreeNodeCreateFolder(const char* path);
TreeNode*   TreeNodeCreateImage(const char* path);
void        TreeNodeDestroy(TreeNode* node);
void        TreeNodeAdd(TreeNode* parent, TreeNode* node);
TreeNode*   TreeNodeFindChild(TreeNode* node, const char* path);

// Clone a single node, but with no sibling/child
TreeNode*   TreeNodeClone(TreeNode* node);

// Recursively clone a tree
TreeNode*   TreeNodeTreeClone(TreeNode* root);

// Recursively destroy a tree
void        TreeNodeTreeDestroy(TreeNode* root);
