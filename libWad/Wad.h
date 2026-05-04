#ifndef WAD_H
#define WAD_H
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// POSIX INCLUDES!
#include <fcntl.h>
#include <unistd.h>
#pragma once

// ANT MESSAGE!: code inspired + used from spec, discussion slides, and fuse tutorials from spec resource links & online

using namespace std;

class Wad {
    // header info (always exactly 12 bytes) magic: 4 byte ascii, 4, 4
    string magic; // ascii (File Type)
    int totalDescriptors; // numerical value (descriptors = elements)
    int descriptorOffset; // numerical value

    string wadPath; // keeps path for reopening rw calls

// USING N-ary tree like discussion reccommmends  
// You should be storing this information in an N-ary tree of some kind of struct, where each struct 
// contains the file name, offset, length, and a way to store other files if a given descriptor is a directory.
public:
    // inspo from spec
    struct Node {
        // node is an element: file or dir
        string name;
        int offset;
        int length;

        bool isDirectory;
        string type; // strings can be root, namespace, map, lump

        // index in descriptor list, helps maintain flattened list order like spec shows when convering to tree
        int index; // (also added because of error with writetofile on bigtest)

        // using vector since child order is preserved (E#M# -> 10 children in order)
        vector<Node*> children; // also easier than ptrs
        Node* parent;

        Node(string name, int offset, int length, bool isDirectory, string type);
    };

    Node* root = nullptr;
    unordered_map<string, Node*> pathMap; 

    static Wad* loadWad(const string &path);
    string getMagic();
    bool isContent(const string &path);
    bool isDirectory(const string &path);
    int getSize(const string &path);
    int getContents(const string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const string &path, vector<string> *directory);
    void createDirectory(const string &path);
    void createFile(const string &path);
    int writeToFile(const string &path, const char *buffer, int length, int offset = 0);

    ~Wad();

private:
    Wad(const string &path); // only called via loadWad
    void buildTree(char* descriptors);
    Node* findNode(const string &path);
    bool isMapMarker(const string &name);
    bool isNamespaceStart(const string &name);
    bool isNamespaceEnd(const string &name);
    void shiftBytes(int fromOffset, int amount);
    void deleteTree(Node* node);
    string cleanPath(const string &path);
    string readName(int fd, int descListStart, int index);
    int findLastSlash(const string &path);
    int findEndMarker(Node* parent);
    string buildPathTree(const vector<Node*> &dirStack, const string &childName);
    string buildPathCreateElement(const string &parentPath, const string &childName);
};

#endif // WAD_H