#include <iostream>
#include <sstream>
#include "fs.h"


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    nextIntreyIndex = 0;
}

FS::~FS()
{

}

int16_t* FS::findFreeBlocks(int fileSize){
    int numOfBlocks;
    if(! (fileSize % BLOCK_SIZE == 0)){
        numOfBlocks = (fileSize/BLOCK_SIZE)+1;
    }
    else
    {
        numOfBlocks = fileSize / BLOCK_SIZE;
    }
    
    int16_t* freeBlocks = new int16_t [numOfBlocks];

    for(int i = 0; i != numOfBlocks; i ++ ){
        for(int j = 0; j != BLOCK_SIZE /2; j ++){
            if(fat[j] == 0){
                freeBlocks[i] = j;
                fat[j] = FAT_EOF;
                break;
            }

        }
    }
    for(int i = 0; i != numOfBlocks; i ++){
        if(i != numOfBlocks ){
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
    char fileOtput[BLOCK_SIZE];
    int i = 0;
    int firstBlock = findFreeBlock();
    bool firstBlockUsed = false;
    while (linestream.get(c))
    {
        if(i == BLOCK_SIZE - 1){
            if(!firstBlockUsed){
                disk.write(firstBlock, (uint8_t*) &fileOtput);
                firstBlockUsed = true;
            }
            else
            {
                disk.write(findFreeBlock(), (uint8_t*) &fileOtput);
            }
            i = 0;
        }
        fileOtput[i] = c;
        i++;
    }
    if(i != 0){
        if(!firstBlockUsed){
                disk.write(firstBlock, (uint8_t*) &fileOtput);
                firstBlockUsed = true;
            }
        else
        {
            disk.write(findFreeBlock(), (uint8_t*) &fileOtput);
        }
        
    }
    dir_entry newEntry;
    for(int i = 0; i != filepath.length(); i ++){
        newEntry.file_name[i]= filepath[i];
    }
    newEntry.first_blk = (uint16_t)firstBlock;
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
    std::cout << "FS::cat(" << filepath << ")\n";
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
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
