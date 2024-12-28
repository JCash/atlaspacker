// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "tree.h"
#include "hash.h"

#include <assert.h>
#include <stdlib.h> // malloc
#include <string.h> // memset

static TreeNode* AllocTreeNode(const char* path)
{
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    memset(node, 0, sizeof(*node));
    node->path = strdup(path);
    node->path_hash = Hash(path);
    return node;
}

static void FreeTreeNode(TreeNode* node)
{
    free((void*)node);
}

void TreeNodeDestroy(TreeNode* node)
{
    FreeTreeNode(node);
}

void TreeNodeAdd(TreeNode* parent, TreeNode* node)
{
    // find last child
    if (!parent->child)
        parent->child = node;
    else
    {
        TreeNode* last = parent->child;
        while (last->sibling)
        {
            last = last->sibling;
        }
        last->sibling = node;
    }
}

TreeNode* TreeNodeCreateFolder(const char* path)
{
    TreeNode* node = AllocTreeNode(path);
    node->type = TN_TYPE_FOLDER;
    node->readonly = 1; // cannot remove a folder
    return node;
}

TreeNode* TreeNodeCreateImage(const char* path)
{
    TreeNode* node = AllocTreeNode(path);
    node->type = TN_TYPE_IMAGE;
    return node;
}

TreeNode* TreeNodeClone(TreeNode* node)
{
    TreeNode* c = AllocTreeNode(node->path);
    c->type = node->type;
    return c;
}

TreeNode* TreeNodeFindChild(TreeNode* node, const char* path)
{
    for (TreeNode* child = node->child; child != 0; child = child->sibling)
    {
        if (strcmp(path, child->path) == 0)
            return child;
    }
    return 0;
}

// Recursively clone a tree
static void TreeNodeTreeCloneRecursive(TreeNode* new_parent, TreeNode* node)
{
    for (TreeNode* child = node->child; child != 0; child = child->sibling)
    {
        TreeNode* new_child = TreeNodeClone(child);
        TreeNodeAdd(new_parent, new_child);

        if (child->child)
            TreeNodeTreeCloneRecursive(new_child, child->child);
    }
}

TreeNode* TreeNodeTreeClone(TreeNode* root)
{
    assert(root != 0);
    TreeNode* newroot = TreeNodeClone(root);
    TreeNodeTreeCloneRecursive(newroot, root);
    return newroot;
}

void TreeNodeTreeDestroy(TreeNode* node)
{
    assert(node != 0);
    TreeNode* child = node->child;
    TreeNodeDestroy(node);

    while (child)
    {
        TreeNode* sibling = child->sibling;
        TreeNodeTreeDestroy(child);
        child = sibling;
    }
}
