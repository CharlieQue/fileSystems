#include <iostream>
#include <sstream>
#include "fs.h"


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    nextIntreyIndex = 0;
    for(int i = 0; i != BLOCK_SIZE /sizeof(dir_entry); i++){
        root_dir[i].type = 2;
    }
}

FS::~FS()
{

}

int* FS::findFreeBlocks(size_t fileSize){
    int numOfBlocks;
    if(! (fileSize % BLOCK_SIZE == 0)){
        numOfBlocks = (fileSize/BLOCK_SIZE)+1;
    }
    else
    {
        numOfBlocks = fileSize / BLOCK_SIZE;
    }
    
    int* freeBlocks = new int [numOfBlocks];

    for(int i = 0; i != numOfBlocks; i ++ ){
        
        for(int j = 0; j != BLOCK_SIZE /2; j ++){
            if(fat[j] == FAT_FREE){
                freeBlocks[i] = j;
                fat[j] = FAT_EOF;
                break;
            }

        }
    }

    for(int i = 0; i != numOfBlocks; i ++){
        if(i != numOfBlocks - 1 ){
            fat[freeBlocks[i]] = freeBlocks[i+1]; 
        }
        
    }

    return freeBlocks; 
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";
    for(unsigned i = 0; i != (BLOCK_SIZE / 2); i++){
        if(i == ROOT_BLOCK){
            fat[ROOT_BLOCK] = FAT_EOF;
        }
        else if(i == FAT_BLOCK){
            fat[FAT_BLOCK] = FAT_EOF;
        }
        else if(i != ROOT_BLOCK || i != FAT_BLOCK){
            fat[i] = FAT_FREE;
        }
    }
    disk.write(FAT_BLOCK,(uint8_t*)&fat);
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";
    std::string line;
    char c;
    std::getline(std::cin, line);
    std::stringstream linestream(line);
    char fileOtput[line.size()];
    int* freeBlocks = findFreeBlocks(line.size());
    int streamSize = 0;
    int freeBlocksIndex = 0;
    while (linestream.get(c))
    {
        if(streamSize == BLOCK_SIZE - 1){
            
            disk.write(freeBlocks[freeBlocksIndex], (uint8_t*) &fileOtput);
            streamSize = 0;
            std::memset(fileOtput,0,BLOCK_SIZE);
            freeBlocksIndex ++;
            
        }
        fileOtput[streamSize] = c;
        streamSize++;
    }
    if(streamSize != 0){
        
        disk.write(freeBlocks[freeBlocksIndex], (uint8_t*) &fileOtput);
        
    }
    disk.write(FAT_BLOCK,(uint8_t*)&fat);
    free(freeBlocks);
    dir_entry newEntry;
    for(int i = 0; i != 56; i ++){
        if(i != filepath.length() ){
            newEntry.file_name[i]= filepath[i];
        }
        else
        {
            newEntry.file_name[i]= '\0';
        }
        
    }    
    newEntry.first_blk = (uint16_t)freeBlocks[0];
    newEntry.size = (uint32_t)line.length();
    newEntry.type = 0;
    newEntry.access_rights = READ|WRITE;
    root_dir[nextIntreyIndex] = newEntry;
    nextIntreyIndex ++;
    disk.write(ROOT_BLOCK,(uint8_t*)&root_dir);
    
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    bool fileExists = false;
    uint16_t firsBlock;
    uint32_t fileSize;
    char fileName[56];
    for(int i = 0; i != 56; i ++){
        if (i != filepath.length()){
            fileName[i]= filepath[i];
        }
        else
        {
            fileName[i] = '\0';
        }
        
    }
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(std::strcmp(root_dir[i].file_name,fileName) == 0){
            fileExists = true;
            firsBlock = root_dir[i].first_blk;
            fileSize = root_dir[i].first_blk;
            break;
        }
    }
    if(!fileExists){
        std::cout << "The file " << filepath << " do not exist!" << std::endl;
        return -1;
    }
    std::cout << "FS::cat(" << filepath << ")\n";
    int numberOfBlocks;
    if(! (fileSize % BLOCK_SIZE == 0)){
        numberOfBlocks = (fileSize / BLOCK_SIZE )+1;
    }
    else
    {
        numberOfBlocks = fileSize / BLOCK_SIZE;
    }
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = firsBlock;
    for (int i = 1; i != numberOfBlocks; i++)
    {
        fileBlocks[i] = fat[fileBlocks[i-1]];
    }

    for(int i = 0; i != numberOfBlocks; i++){
        char fileInput[BLOCK_SIZE];
        disk.read(fileBlocks[i],(uint8_t*) & fileInput);
        std::cout << fileInput << std::flush;

    }
    std::cout << std::endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
    std::cout << "Filename" <<  "     " << "Size" << std::endl;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(root_dir[i].type != 2){
            std::cout << root_dir[i].file_name <<  "         " << root_dir[i].size << std::endl;
        }
    }
    return 0;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int
FS::cp(std::string sourcefilepath, std::string destfilepath)
{
    std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
