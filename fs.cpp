#include <iostream>
#include <sstream>
#include "fs.h"


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK,(uint8_t*) & fat);
    disk.read(ROOT_BLOCK,(uint8_t*) & root_dir);
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
    for(int i = 0; i != numOfBlocks ; i ++){
        freeBlocks[i] = -1;
    }

    for(int i = 0; i != numOfBlocks; i ++ ){
        
        for(int j = 0; j != BLOCK_SIZE /2; j ++){
            if(fat[j] == FAT_FREE){
                freeBlocks[i] = j;
                fat[j] = FAT_EOF;
                break;
            }

        }
    }
    if(freeBlocks[numOfBlocks -1] == -1){
        for(int i = 0; i != numOfBlocks ; i ++){
            if(freeBlocks[i] == -1){
                fat[freeBlocks[i]] = FAT_FREE;
            }
        }
        free(freeBlocks);
        return nullptr;
    }

    for(int i = 0; i != numOfBlocks; i ++){
        if(i != numOfBlocks - 1 ){
            fat[freeBlocks[i]] = freeBlocks[i+1]; 
        }
        
    }

    return freeBlocks; 
}

int* 
FS::appendBlocks(uint16_t firstBlock,size_t fileSize){
    int* freeBlocks = findFreeBlocks(fileSize);
    if(freeBlocks == nullptr){
        return nullptr;
    }
    int lastBlock = firstBlock;
    while (fat[lastBlock] != FAT_EOF ){
        lastBlock = fat[lastBlock];
    }
    fat[lastBlock] = freeBlocks[0];
    return freeBlocks;

}

bool 
FS::fileExists(uint16_t & firstBlock, uint32_t & fileSize, int & numberOfBlocks,std::string filepath, uint16_t rootBlock){
    bool fileExists = false;
    dir_entry current_dir[(BLOCK_SIZE / sizeof(dir_entry)) +1];
    disk.read(rootBlock,(uint8_t*)&current_dir);
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(current_dir[i].file_name == filepath){
            fileExists = true;
            firstBlock = current_dir[i].first_blk;
            fileSize = current_dir[i].size;
            break;
        }
    }
    if(!fileExists){
        if(fat[rootBlock] != FAT_EOF){
            FS::fileExists(firstBlock,fileSize,numberOfBlocks,filepath,fat[rootBlock]);
        }
        return false;
    }
    if(! (fileSize % BLOCK_SIZE == 0)){
        numberOfBlocks = (fileSize / BLOCK_SIZE )+1;
    }
    else
    {
        numberOfBlocks = fileSize / BLOCK_SIZE;
    }
    return true;

}
bool
FS::entryInit(std::string filepath, size_t fileSize, uint16_t firstBlock, uint16_t rootBlock){
    dir_entry newEntry;
    std::fill_n(newEntry.file_name, 56, '\0');
    for(int i = 0; i != filepath.length(); i ++){
        newEntry.file_name[i]= filepath[i];
    }
    newEntry.first_blk = firstBlock;
    newEntry.size = fileSize;
    newEntry.type = 0;
    newEntry.access_rights = READ|WRITE;
    int freeIntreyIndex = -1;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(strlen(root_dir[i].file_name) == 0){
            freeIntreyIndex = i;
            break;
        }
    }
    if(freeIntreyIndex == -1){

        int * nextRootBlock = appendBlocks(ROOT_BLOCK,BLOCK_SIZE);
        if(nextRootBlock == nullptr){
            return false;
        }        
        memset(root_dir,0,(BLOCK_SIZE / sizeof(dir_entry)) +1); 
        entryInit(filepath,fileSize,firstBlock, nextRootBlock[0]);
    }
    root_dir[freeIntreyIndex] = newEntry;
    disk.write(rootBlock,(uint8_t*)&root_dir);
    return true;

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
    char fileOtput[BLOCK_SIZE];
    std::fill_n(fileOtput,BLOCK_SIZE, '\0');
    int* freeBlocks = findFreeBlocks(line.size());
    if(freeBlocks == nullptr){
        std::cout << "The file can't be created, no enough space on disk" << std::endl;
        return -1;
    }
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
    entryInit(filepath,line.length(),freeBlocks[0]);
    free(freeBlocks);
    
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    
    uint16_t firsBlock;
    uint32_t fileSize;
    int numberOfBlocks;
    if(!fileExists(firsBlock,fileSize,numberOfBlocks,filepath)){
        std::cout << "The file " << filepath << " do not exist!" << std::endl;
        return -1;
    }
    std::cout << "FS::cat(" << filepath << ")\n";
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
    if(fat[ROOT_BLOCK] != FAT_EOF){
        dir_entry temp[(BLOCK_SIZE / sizeof(dir_entry)) + 1];
        int lastBlock = ROOT_BLOCK;
        while (fat[lastBlock] != FAT_EOF ){
            disk.read(lastBlock,(uint8_t*) &temp);
            for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
                if(strlen(temp[i].file_name) != 0){
                    std::cout << temp[i].file_name <<  "         " << temp[i].size << std::endl;
                }
            }
            memset(temp,0,(BLOCK_SIZE / sizeof(dir_entry)));
            lastBlock = fat[lastBlock];
        }
        
    }
    else
    {
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(strlen(root_dir[i].file_name) != 0){
                std::cout << root_dir[i].file_name <<  "         " << root_dir[i].size << std::endl;
            }
    }
        
    }
    
    
    return 0;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int
FS::cp(std::string sourcefilepath, std::string destfilepath)
{
    uint16_t firsBlock;
    uint32_t fileSize;
    int numberOfBlocks;
    if(!fileExists(firsBlock,fileSize,numberOfBlocks,sourcefilepath)){
        std::cout << "The file " << sourcefilepath << " do not exist!" << std::endl;
        return -1;
    }
    std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = firsBlock;
    for (int i = 1; i != numberOfBlocks; i++)
    {
        fileBlocks[i] = fat[fileBlocks[i-1]];
    }

    char fileInput[numberOfBlocks][BLOCK_SIZE];
    for(int i = 0; i != numberOfBlocks; i++){
        disk.read(fileBlocks[i],(uint8_t*) & fileInput[i]);
    }
    int* freeBlocks = findFreeBlocks(fileSize);
    if(freeBlocks == nullptr){
        std::cout << "The file can't be copied, no enough space on disk" << std::endl;
        return -1;
    }
    for(int i = 0; i != numberOfBlocks; i++){
        disk.write(freeBlocks[i],(uint8_t*)&fileInput[i]);
    }
    disk.write(FAT_BLOCK,(uint8_t*)&fat);
    entryInit(destfilepath,fileSize,firsBlock);
    






    free(freeBlocks);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    std::cout << BLOCK_SIZE / sizeof(dir_entry) << std::endl;
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
