#include "Wad.h"
#include <iostream>
#include <cstring>
#include <sys/stat.h>

using namespace std;


// --------------------------------------------------------  WAD CLASS -----------------------------------------------------------------------

// Private constructor, takes in path to a WAD file from your real file system.
// Reads the header data and initializes data structure(s) to represent the WAD file elements from the descriptor list.
Wad::Wad(const string &path){
    this->wadPath = path;

    // reading wad file
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0){
        return; // exits if file cant be read / error
    }

    // HEADER:
    // always 12 bytes
    // magic (file type) numofdescriptors descriptoroffset | 4 4 4
    char buffer[4]; // reads magic and copies to buffer to assign
    
    // using pread since read() requires lseek
    pread(fd, buffer, 4, 0);
    this->magic = string(buffer, 4);

    pread(fd, &this->totalDescriptors, 4, 4);
    pread(fd, &this->descriptorOffset, 4, 8);

    // DESCRIPTORS: 
    // element offset, element length, name | 4 4 8
    // always 16 bytes total
    // read the entire descriptor list into buffer
    int dlistSize = totalDescriptors * 16;

    char* descriptors = new char[dlistSize];
    pread(fd, descriptors, dlistSize, descriptorOffset);

    close(fd);

    // root node (spec says root of all paths is "/")
    root = new Node("", 0, 0, true, "root");
    root->parent = nullptr;
    pathMap["/"] = root;

    // build the actual tree from the flat descriptor list
    buildTree(descriptors);

    // done with the temp buffer, avoids mem leak!!!!!
    delete[] descriptors;
}

// Object allocator; dynamically creates a Wad object and loads the WAD file data from path into memory.
// The caller is responsible for deallocating the memory using the delete keyword.
// Invokes the constructor by creating a Wad object with new, then returns the pointer to it.
Wad* Wad::loadWad(const string &path){
    Wad* wad = new Wad(path);
    return wad;
}

// destructor
Wad::~Wad(){
    deleteTree(root);
    root = nullptr;
}

// --------------------------------------------------------  N-Ary Tree Structure Functions ------------------------------------------------------------------------------------

Wad::Node::Node(string name, int offset, int length, bool isDirectory, string type){
    this->name = name;
    this->offset = offset;
    this->length = length;
    this->isDirectory = isDirectory;
    this->type = type;
    this->parent = nullptr;
    this->index = -1;
}

// post order traversal, deletes children before parent
// (inspo from avl tree project)
void Wad::deleteTree(Node* node){
    if (!node){
        return;
    }
    for (int i = 0; i < node->children.size(); i++){
        deleteTree(node->children[i]);
    }
    delete node;
}

// You must traverse the flattened tree that is your descriptor list and convert it into an actual tree
// stack based DFS: push on _START / E#M#, pop on _END or map counter hits 0
void Wad::buildTree(char* descriptors){
    // stack of current directory context
    vector<Node*> dirStack;
    dirStack.push_back(root);

    // map marker counter: when set, descriptors belong to map dir at top of stack
    int mapCounter = 0;

    for (int i = 0; i < totalDescriptors; i++){
        int descOffset = i * 16;

        // parse 4 offset + 4 length + 8 name
        int elemOffset = 0;
        int elemLength = 0;
        memcpy(&elemOffset, &descriptors[descOffset], 4);
        memcpy(&elemLength, &descriptors[descOffset + 4], 4);

        string name = "";

        // adds to name string until 8th byte
        for (int j = 0; j < 8; j++){
            if (descriptors[descOffset + 8 + j] == 0){
                break;
            } 
            name += descriptors[descOffset + 8 + j];
        }

        Node* currDir = dirStack[dirStack.size() - 1];

        // CHECK MARKERS!
        if (isNamespaceEnd(name)){
            // pop out of namespace dir
            if (dirStack.size() > 1){
                dirStack.pop_back();
            }
            continue;
        }

        if (isNamespaceStart(name)){
            // create namespace dir, name strips "_START"
            string dirName = name.substr(0, name.length() - 6);
            Node* newDir = new Node(dirName, elemOffset, elemLength, true, "namespace");
            newDir->index = i;
            newDir->parent = currDir;
            currDir->children.push_back(newDir);

            // key: string path 
            // value: node
            string namespacePath = buildPathTree(dirStack, dirName);
            pathMap[namespacePath] = newDir;

            dirStack.push_back(newDir); 
            continue;                   
        }

        if (isMapMarker(name)){
            // create map dir, next 10 descriptors are children
            Node* newDir = new Node(name, elemOffset, elemLength, true, "map");
            newDir->index = i;
            newDir->parent = currDir;
            currDir->children.push_back(newDir);

            string mapPath = buildPathTree(dirStack, name);
            pathMap[mapPath] = newDir;

            dirStack.push_back(newDir); 
            mapCounter = 10;            // next 10 descriptors belong to this map
            continue;                   
        }

        // regular content file
        Node* newFile = new Node(name, elemOffset, elemLength, false, "lump");
        newFile->index = i;
        newFile->parent = currDir;
        currDir->children.push_back(newFile);

        string lumpPath = buildPathTree(dirStack, name);
        pathMap[lumpPath] = newFile;

        // if in a map dir, count down
        if (mapCounter > 0){
            mapCounter--;
            if (mapCounter == 0){
                // exited the map dir, pop it off
                dirStack.pop_back();
            }
        }
    }
}

// normalize path and look up in map
Wad::Node* Wad::findNode(const string &path){
    if (path.empty()){
        return nullptr;
    }

    // root case
    if (path == "/"){
        return root;
    }

    // removes final / if present 
    // /F/F1/E1M1/ -> /F/F1/E1M1
    string cleanedPath = cleanPath(path);

    // uses .find() on map for quick lookup of node to return
    auto it = pathMap.find(cleanedPath);
    if (it == pathMap.end()){
        return nullptr;
    }
    return it->second;
}

// --------------------------------------------------------------------------  READ FUNCTIONS ---------------------------------------------------------------------------------------

// Returns true if path represents content, false otherwise.
bool Wad::isContent(const string &path){
    Node* node = findNode(path);
    if (!node){
        return false;
    }
    return (node->type == "lump");
}

// Returns true if path represents a directory, false otherwise.
bool Wad::isDirectory(const string &path){
    Node* node = findNode(path);
    if (!node){
        return false;
    }
    return node->isDirectory;
}

// -----------------------------------------------------------------------------  WRITE FUNCTIONS ---------------------------------------------------------------

// Takes in path to a directory that does not yet exist and must be created.
// New directories can only be created in namespace directories.
void Wad::createDirectory(const string &path){
    string cleanedPath = cleanPath(path);

    // split new dir name from parent path
    int lastSlashIndex = findLastSlash(cleanedPath);
    if (lastSlashIndex == -1){
        return; // invalid path
    }

    string parentPath = cleanedPath.substr(0, lastSlashIndex);
    string createdDirName = cleanedPath.substr(lastSlashIndex + 1);

    if (parentPath.empty()){
        parentPath = "/"; // root case
    }

    // name must be 2 chars max: 8 - "_START"(6chars) = 2
    if (createdDirName.length() > 2 || createdDirName.empty()){
        return;
    }

    // parent must exist and be a valid writable dir
    Node* parent = findNode(parentPath);
    if (!parent){
        return;
    }
    // cant create in map dir or in lump
    if (parent->type == "map" || parent->type == "lump"){
        return;
    }

    // finds directory's insert position located before parent's end marker
    int createdDirIndex = findEndMarker(parent);

    // directory's byte position in memory to insert
    int createdDirMemPos = descriptorOffset + createdDirIndex * 16;

    // shift bytes forward by 32 bytes (2 descriptors for _START _END)
    // descriptor is 16 bytes -> *2 -> 32 bytes
    shiftBytes(createdDirMemPos, 32);

    // pads 0s for offset and length
    char namespaceDescriptors[32];
    for (int i = 0; i < 32; i++){
        namespaceDescriptors[i] = 0;
    }

    // START descriptor
    string start = createdDirName + "_START";
    for (int i = 0; i < (int)start.length(); i++){
        // name goes in bytes 8-15
        namespaceDescriptors[8 + i] = start[i];
    }

    // END descriptor: 
    string end = createdDirName + "_END";
    for (int i = 0; i < (int)end.length(); i++){
        // name goes in bytes 24-31
        namespaceDescriptors[24 + i] = end[i];
    }

    // write into the gap
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0){
        return;
    }
    pwrite(fd, namespaceDescriptors, 32, createdDirMemPos);

    // UPDATE HEADER!
    totalDescriptors += 2;
    pwrite(fd, &totalDescriptors, 4, 4);

    // added inside the desc list so descoffset doesnt change 
    close(fd);

    // UPDATE TREE AND MAP!
    Node* newDir = new Node(createdDirName, 0, 0, true, "namespace");
    newDir->parent = parent;
    parent->children.push_back(newDir);

    // add 2 to all nodes' index if inserted at or after createdDirIndex
    for (auto it = pathMap.begin(); it != pathMap.end(); it++){
        if (it->second->index >= createdDirIndex){
            it->second->index += 2;
        }
    }

    newDir->index = createdDirIndex; // _START gets createdDirIndex, _END is createdDirIndex+1

    // creates new path for new dir
    string dirPath = buildPathCreateElement(parentPath, createdDirName);
    pathMap[dirPath] = newDir;
}

// Takes in path to a file that does not yet exist and must be created.
// New files can only be created in namespace directories.
void Wad::createFile(const string &path){
    string cleanedPath = cleanPath(path);
    // split new file name from parent path
    int lastSlashIndex = findLastSlash(cleanedPath);
    if (lastSlashIndex == -1){
        return; // invalid path
    }
    string parentPath = cleanedPath.substr(0, lastSlashIndex);
    string createdFileName = cleanedPath.substr(lastSlashIndex + 1);
    if (parentPath.empty()){
        parentPath = "/"; // root case
    }
    // name must be 8 chars max
    if (createdFileName.length() > 8 || createdFileName.empty()){
        return;
    }
    // make sure name isnt a marker
    if (isMapMarker(createdFileName) || isNamespaceStart(createdFileName) || isNamespaceEnd(createdFileName)){
        return;
    }
    // parent must exist and be a valid writable dir
    Node* parent = findNode(parentPath);
    if (!parent){
        return;
    }
    // cant create in map dir or in lump
    if (parent->type == "map" || parent->type == "lump"){
        return;
    }
    // finds file's insert position located before parent's end marker
    int createdFileIndex = findEndMarker(parent);
    // file's byte position in memory to insert
    int createdFileMemPos = descriptorOffset + createdFileIndex * 16;
    // shift bytes forward by 16 bytes (1 descriptor for a file)
    shiftBytes(createdFileMemPos, 16);
    // pads 0s for offset and length
    char fileDescriptor[16];
    for (int i = 0; i < 16; i++){
        fileDescriptor[i] = 0;
    }
    // name goes in bytes 8-15
    for (int i = 0; i < (int)createdFileName.length(); i++){
        fileDescriptor[8 + i] = createdFileName[i];
    }
    // write into the gap
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0){
        return;
    }
    pwrite(fd, fileDescriptor, 16, createdFileMemPos);
    // UPDATE HEADER!
    totalDescriptors += 1;
    pwrite(fd, &totalDescriptors, 4, 4);
    close(fd);
    // UPDATE TREE AND MAP!
    Node* newFile = new Node(createdFileName, 0, 0, false, "lump");
    newFile->parent = parent;
    parent->children.push_back(newFile);
    // add 1 to all nodes' index if inserted at or after createdFileIndex
    for (auto it = pathMap.begin(); it != pathMap.end(); it++){
        if (it->second->index >= createdFileIndex){
            it->second->index += 1;
        }
    }
    newFile->index = createdFileIndex;
    // creates new path
    string newFilePath = buildPathCreateElement(parentPath, createdFileName);
    pathMap[newFilePath] = newFile;
}

// If path is an empty existing file, writes length bytes from buffer into lump data.
// Returns bytes written or -1 if path isnt content. Returns 0 if file is non-empty.
int Wad::writeToFile(const string &path, const char *buffer, int length, int offset){
    if (!isContent(path)){
        return -1;
    }
    Node* node = findNode(path);

    // non-empty file -> return 0
    if (node->length != 0){
        return 0;
    }

    // make space in lump data section: shift descriptor list forward by length bytes
    shiftBytes(descriptorOffset, length);

    // write buffer into the new gap (which starts where descriptorOffset used to be)
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0){
        return -1;
    }

    int lumpPos = descriptorOffset; // old descriptor start = new lump position
    pwrite(fd, buffer, length, lumpPos);

    // update node in memory
    node->offset = lumpPos;
    node->length = length;

    // find the descriptor in the file and update offset + length
    // descriptor list is now at descriptorOffset + length
    // use index directly to avoid ambiguity with same-named files in different dirs
    int newDescListStart = descriptorOffset + length;
    int descBytePos = newDescListStart + node->index * 16;
    pwrite(fd, &node->offset, 4, descBytePos);
    pwrite(fd, &node->length, 4, descBytePos + 4);

    // UPDATE HEADER! descriptor offset moved forward by length
    descriptorOffset = newDescListStart;
    pwrite(fd, &descriptorOffset, 4, 8);

    close(fd);

    return length;
}

// ----------------------------------------------------------------------------- HELPERS ------------------------------------------------------------------------------------

// strips trailing slash from path
// /F/F1/E1M1/ -> /F/F1/E1M1
string Wad::cleanPath(const string &path){
    if (path.empty()){
        return path;
    }
    string clean = path;
    if (clean.back() == '/'){
        clean.pop_back();
    }
    return clean;
}

// reads the 8-byte name field of descriptor at index from the WAD file
// descListStart is the byte offset where the descriptor list begins
string Wad::readName(int fd, int descListStart, int index){
    char nameBuffer[9];
    for (int j = 0; j < 9; j++){
        nameBuffer[j] = 0;
    }
    // name field starts at byte 8 within each 16-byte descriptor
    pread(fd, nameBuffer, 8, descListStart + index * 16 + 8);
    string name = "";
    for (int j = 0; j < 8; j++){
        if (nameBuffer[j] == 0){
            break;
        }
        name += nameBuffer[j];
    }
    return name;
}

// E#M# format where # is a digit 
// (e.g. E1M0, E3M9)
bool Wad::isMapMarker(const string &name){
    if (name.length() != 4){
        return false;
    }
    bool firstE = (name[0] == 'E');
    bool secondDigit = isdigit(name[1]);
    bool thirdM = (name[2] == 'M');
    bool fourthDigit = isdigit(name[3]);

    if(firstE && secondDigit && thirdM && fourthDigit){ // IF E # M # are all valid
        return true;
    }
    return false;
}

// ends in "_START"
bool Wad::isNamespaceStart(const string &name){
    if (name.length() < 7){
        return false; // need at least 1 char name + "_START"
    }
    int startIndex = name.length() - 6;
    string suffix = name.substr(startIndex);
    return (suffix == "_START");
}

// ends in "_END"
bool Wad::isNamespaceEnd(const string &name){
    if (name.length() < 5){
        return false; // need at least 1 char name + "_END"
    }
    int startIndex = name.length() - 4;
    string suffix = name.substr(startIndex);
    return (suffix == "_END");
}

// helper for createDirectory / createFile / writeToFile
void Wad::shiftBytes(int fromOffset, int amount){
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0){
        return;
    }

    // NEED FILE SIZE!
    // fstat fills struct stat (kernel file metadata) for fd to get size
    // found via posix documentation at https://man7.org/linux/man-pages/man3/fstat.3p.html
    struct stat st;
    fstat(fd, &st);
    int fileSize = st.st_size;

    // shift bytes backwards from end to fromOffset, moving each byte forward by amount
    // reads backwards to avoid overwriting bytes not copied yet!!!!!!!
    for (int i = fileSize - 1; i >= fromOffset; i--){
        char byte;
        pread(fd, &byte, 1, i);
        pwrite(fd, &byte, 1, i + amount);
    }
    close(fd);
}

// helper function to determine last slash so parents paths and child path can be determined for naming
// "/F/F1/F2" -> lastSlash = 5 so parent = "/F/F1" and child = "F2"
// returns -1 if no slash found (caller treats as invalid path)
int Wad::findLastSlash(const string &path){
    int lastSlash = -1;
    for (int i = 0; i < (int)path.length(); i++){
        if (path[i] == '/'){
            lastSlash = i;
        }
    }
    return lastSlash;
}

// builds full path for tree from stack and child name
string Wad::buildPathTree(const vector<Node*> &dirStack, const string &childName){
    string fullPath = "";
    // skip root
    for (int j = 1; j < (int)dirStack.size(); j++){
        fullPath += "/" + dirStack[j]->name;
    }
    fullPath += "/" + childName;
    return fullPath;
}

// builds full path from parent path string and child name
string Wad::buildPathCreateElement(const string &parentPath, const string &childName){
    if (parentPath == "/"){
        return "/" + childName;
    }
    return parentPath + "/" + childName;
}

// iterates through descriptor list from parent's _START forward to find its matching _END
int Wad::findEndMarker(Node* parent){
    // returns totalDescriptors since only root
    if (parent->type == "root"){
        return totalDescriptors;
    }

    int fd = open(wadPath.c_str(), O_RDONLY);

    // invalid file so just returns totalDescriptors in Wad
    if (fd < 0){
        return totalDescriptors;
    }

    // 0 signifies level begins at parent
    int level = 0;
    int endIndex = totalDescriptors; // safe fallback

    // iterate through entire flattened descriptor list
    for (int i = parent->index + 1; i < totalDescriptors; i++){
        string name = readName(fd, descriptorOffset, i);

        // new level / nested content
        if (isNamespaceStart(name)){
            level++;
        }

        // if level returns to 0, i is the index of the parent node's end marker
        if (isNamespaceEnd(name)){
            if (level == 0){
                endIndex = i;
                break;
            }
            // if end is reached, "go up" a level
            level--;
        }
    }
    close(fd);
    return endIndex;
}

// -----------------------------------------------------------------------------  GETTERS AND SETTERS ------------------------------------------------------------------------------------
// some of these are read functions too but they say get so its easy to identify :P


// returns magic string (file type) from this wad
string Wad::getMagic(){
    return magic;
}

// If path represents content, returns the number of bytes in its data; otherwise returns -1.
int Wad::getSize(const string &path){
    if (!isContent(path)){
        return -1;
    }
    Node* node = findNode(path);
    return node->length;
}

// If path represents a directory, pushes child names into directory vector.
// Returns number of children, or -1 if path is not a directory.
int Wad::getDirectory(const string &path, vector<string> *directory){
    if (!isDirectory(path)){
        return -1;
    }
    Node* node = findNode(path);
    for (int i = 0; i < (int)node->children.size(); i++){
        directory->push_back(node->children[i]->name);
    }
    return node->children.size();
}

// If path represents content, copies up to length bytes starting at offset into buffer.
// Returns number of bytes copied, or -1 if path is not content.
int Wad::getContents(const string &path, char *buffer, int length, int offset){
    if (!isContent(path)){
        return -1;
    }
    Node* node = findNode(path);

    // case 6: Offset goes beyond end of file, so we cannot copy any bytes.
    // Not an error, but no bytes are copied into buffer. Return 0.
    if (offset >= node->length){
        return 0;
    }

    // case 4/5: Length exceeds size of file, so we only copy what we can
    int available = node->length - offset;
    int numBytesCopied = 0;
    if (length < available){
        numBytesCopied = length;
    } else {
        numBytesCopied = available;
    }

    // open file, pread from node's lump data
    int fd = open(wadPath.c_str(), O_RDONLY);
    if (fd < 0){
        return -1;
    }
    int bytesRead = pread(fd, buffer, numBytesCopied, node->offset + offset);
    close(fd);

    return bytesRead;
}