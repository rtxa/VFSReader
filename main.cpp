#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct VFSFileHeader {
  uint32_t Signature;
  uint16_t Version;         // Version number
  int32_t Dispersed;        // Is this VFS dispersed?
  int32_t DirectoryOffset;  // File offset to directory
  uint32_t DataLength;      // Length of all file data, including VFS header
  uint32_t EndPosition;     // End Position in the RWOps file we were written to
};

struct DirTreeHeader {
  uint32_t Signature;
  int32_t Size;
};

enum geVFile_Attributes { None = 0, ReadOnly = (1 << 0), Directory = (2 << 0) };

struct geVFile_Time {
  uint32_t Time1;
  uint32_t Time2;
};

struct geVFile_Hints {
  uint32_t HintDataLength;
  std::string HintData;
};

struct DirTree {
  std::string Name;
  geVFile_Time Time;
  uint32_t AttributeFlags;
  uint32_t Size;
  uint32_t Offset;
  geVFile_Hints Hints;
  std::unique_ptr<DirTree> Parent;
  std::unique_ptr<DirTree> Children;
  std::unique_ptr<DirTree> Siblings;
};

#define DIRTREE_LIST_TERMINATED 0xFFFFFFFF

// Function to read data from the file
template <typename T>
bool ReadFromFile(std::ifstream& file, T& data) {
  file.read(reinterpret_cast<char*>(&data), sizeof(T));
  return file.good();
}

bool ReadStringFromFile(std::ifstream& file,
                        std::string& outString,
                        int32_t length) {
  if (length <= 0) {
    return false;  // No valid length to read
  }

  outString.resize(length);          // Resize the string to hold the data
  file.read(&outString[0], length);  // Read the data into the string
  return file.good();                // Return true if the read was successful
}

// ReadTree function
bool ReadTree(std::ifstream& file, std::unique_ptr<DirTree>& TreePtr) {
  // Read the terminator value
  int32_t terminator;
  file.read(reinterpret_cast<char*>(&terminator), sizeof(int32_t));

  if (terminator == DIRTREE_LIST_TERMINATED) {
    TreePtr.reset();  // Set TreePtr to nullptr
    return true;
  }

  // Allocate space for the DirTree
  TreePtr = std::make_unique<DirTree>();
  if (!TreePtr) {
    return false;
  }

  // Read the name length
  int32_t nameLength;
  file.read(reinterpret_cast<char*>(&nameLength), sizeof(int32_t));

  ReadStringFromFile(file, TreePtr->Name, nameLength);

  // Read out the attribute information
  if (!ReadFromFile(file, TreePtr->Time) ||
      !ReadFromFile(file, TreePtr->AttributeFlags) ||
      !ReadFromFile(file, TreePtr->Size) ||
      !ReadFromFile(file, TreePtr->Offset) ||
      !ReadFromFile(file, TreePtr->Hints.HintDataLength)) {
    return false;
  }

  int32_t hintLength = TreePtr->Hints.HintDataLength;
  ReadStringFromFile(file, TreePtr->Hints.HintData, hintLength);

  // Read the children
  if (!ReadTree(file, TreePtr->Children)) {
    return false;
  }

  // Read the siblings
  if (!ReadTree(file, TreePtr->Siblings)) {
    return false;
  }

  return true;
}

std::vector<std::string> GetNames(const DirTree* tree) {
  std::vector<std::string> names;
  if (tree) {
    names.push_back(tree->Name);
    if (tree->Children) {
      auto childNames = GetNames(tree->Children.get());
      names.insert(names.end(), childNames.begin(), childNames.end());
    }
    if (tree->Siblings) {
      auto siblingNames = GetNames(tree->Siblings.get());
      names.insert(names.end(), siblingNames.begin(), siblingNames.end());
    }
  }
  return names;
}

int main() {
  VFSFileHeader header;
  DirTreeHeader dirHeader;
  std::unique_ptr<DirTree> root;

  std::ifstream file("gedit.txl", std::ios::in | std::ios::binary);
  file.read(reinterpret_cast<char*>(&header), sizeof(VFSFileHeader));
  file.seekg(header.DirectoryOffset, std::ios::beg);
  file.read(reinterpret_cast<char*>(&dirHeader), sizeof(DirTreeHeader));

  if (ReadTree(file, root) == true) {
    std::cout << "Tree read successfully!" << std::endl;
  } else {
    std::cerr << "Failed to read the tree." << std::endl;
  }

  std::vector<std::string> names = GetNames(root.get());
  for (const auto& name : names) {
    std::cout << "Name: " << name << std::endl;
  }
}
