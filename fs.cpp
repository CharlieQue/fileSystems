#include <iostream>
#include <sstream>
#include "fs.h"


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK,(uint8_t*) & fat);
    disk.read(ROOT_BLOCK,(uint8_t*) & cwd);
    cwdBlock = ROOT_BLOCK;
}

FS::~FS()
{

}


void
FS::pathExtruder(std::string file_path, std::vector <std::string>& pathArgs){
    int startIndex = 0;
    if(file_path[0] == '/'){
        pathArgs.push_back("/");
        startIndex = 1;
    }
    if ( file_path[0] == '.' && file_path[1] == '/'){
        startIndex = 2;
    }
    std::string arg = "";
    for(int i = startIndex ; i != file_path.length(); i++){
        if(file_path[i] != '/'){
            arg += file_path[i];
        }
        else
        {
            pathArgs.push_back(arg);
            arg = "";
        }
        if(i == file_path.length() - 1){
            pathArgs.push_back(arg);
        }
        
    }
}

int*
FS::findFreeBlocks(size_t fileSize){
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
FS::fileExists(std::string filepath , dir_entry & fileEntry){
    bool fileExists = false;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(cwd[i].file_name == filepath){
            fileExists = true;
            fileEntry = cwd[i];
            break;
        }
    }
    if(!fileExists){
        return false;
    }
    return true;

}

bool
FS::entryInit(std::string filename, size_t fileSize, uint16_t firstBlock, int type,int access_rights){
    dir_entry newEntry;
    std::fill_n(newEntry.file_name, 56, '\0');
    for(int i = 0 ; i < filename.length();i ++){
        newEntry.file_name[i] = filename[i];
    }
    newEntry.first_blk = firstBlock;
    newEntry.size = fileSize;
    newEntry.type = type;
    newEntry.access_rights = access_rights;

    int freeEnteryIndex = -1;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(strlen(cwd[i].file_name) == 0){
            freeEnteryIndex = i;
            break;
        }
    }
    if(freeEnteryIndex == -1){
        return false; // no enough space
    }
    cwd[freeEnteryIndex] = newEntry;
    disk.write(cwd[0].first_blk,(uint8_t*)&cwd);
    return true;
}

bool 
FS::accessCheck(uint8_t access_rights, uint8_t requestedAccess){
    std::string accessValues[7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};
    std::string requestedAccessString = accessValues[requestedAccess - 1];
    std::string accessValuesString = accessValues[access_rights - 1];
    for(int i = 0; i < 3; i++){
        if (requestedAccessString[i] != '-'){
            if(requestedAccessString[i] != accessValuesString[i]){
                return false;
            }
        }
    }
    return true;
}

void
FS::sizeUpdater(size_t sizeArgs){
    int savedBlock = cwdBlock;
    int childBlock = cwd[0].first_blk;
    cwd[0].size += sizeArgs;
    disk.write(cwd[0].first_blk,(uint8_t*)&cwd);
    while(childBlock != ROOT_BLOCK ){
        cd("..", true);
        cwd[0].size += sizeArgs;
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].first_blk == childBlock){
                cwd[i].size += sizeArgs;
                childBlock = cwd[0].first_blk;
                disk.write(cwd[0].first_blk,(uint8_t*)&cwd);
                break;
            }
        }
    }
    cwd[0].size += sizeArgs;
    disk.write(cwd[0].first_blk,(uint8_t*)&cwd);
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;

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
    dir_entry newEntry;
    std::fill_n(newEntry.file_name, 56, '\0');
    newEntry.file_name[0] = '.';
    newEntry.first_blk = ROOT_BLOCK;
    newEntry.size = 0;
    newEntry.type = TYPE_DIR;
    newEntry.access_rights = READ | WRITE;
    cwd[0] = newEntry;
    disk.write(ROOT_BLOCK,(uint8_t*)&cwd);
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";
    int savedBlock = cwdBlock;
    std::vector<std::string> pathArgs;
    pathExtruder(filepath, pathArgs);

    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    if(!accessCheck(cwd[0].access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    dir_entry file_entry;
    if(fileExists(pathArgs[pathArgs.size() - 1], file_entry)){
        std::cout << "The file " << pathArgs[pathArgs.size() - 1] << " already exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    std::string line;
    char c;
    std::getline(std::cin, line);
    std::stringstream linestream(line);
    char fileOtput[BLOCK_SIZE];
    std::fill_n(fileOtput,BLOCK_SIZE, '\0');
    int* freeBlocks = findFreeBlocks(line.size());
    if(freeBlocks == nullptr){
        std::cout << "The file can't be created, no enough space on disk" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    if(!entryInit(pathArgs[pathArgs.size() - 1],line.length(),freeBlocks[0],TYPE_FILE)){
        std::cout << "The maximum amount of files in the directory " << pathArgs[pathArgs.size() - 2] << " has been reached" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
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
    
    free(freeBlocks);
    sizeUpdater(line.size());
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    int savedBlock = cwdBlock;
    std::vector<std::string> pathArgs;
    pathExtruder(filepath, pathArgs);

    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    dir_entry file_entry;
    if(!fileExists(pathArgs[pathArgs.size() - 1], file_entry)){
        std::cout << "The file " << pathArgs[pathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    if(!accessCheck(file_entry.access_rights,READ)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }


    if(file_entry.type == TYPE_DIR){
        std::cout << filepath <<" is a directory." << std::endl;
        return -1;
    }
    if(file_entry.size == 0){
        std::cout << std::endl;
        return 0;
    }
    int numberOfBlocks;
    if(! (file_entry.size % BLOCK_SIZE == 0)){
        numberOfBlocks = (file_entry.size / BLOCK_SIZE )+1;
    }
    else
    {
        numberOfBlocks = file_entry.size / BLOCK_SIZE;
    }
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = file_entry.first_blk;
    
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
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
    std::string accessArray [7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};

    std::cout << std::setw(25) << std::left << "Filename" << std::setw(25) << std::left  <<  "Type" << std::setw(25) << std::left << "Access rights" << std::setw(25) << std::left << "Size" << std::endl;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(strlen(cwd[i].file_name) != 0 && (cwd[i].file_name[0] != '.')){
            std::cout << std::setw(25) << std::left << cwd[i].file_name;
            if(cwd[i].type == TYPE_FILE) {
                std::cout << std::setw(25) << std::left << "File";
            }
            else
            {
                std::cout << std::setw(25) << std::left << "Dir";
            }
            std::cout << std::setw(25) << std::left << accessArray[cwd[i].access_rights - 1];
            std::cout << std::setw(25) << std::left << cwd[i].size;
            std::cout << std::endl;
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
    int savedBlock = cwdBlock;

    std::vector<std::string> sourcePathArgs;
    std::vector<std::string> destPathArgs;
    dir_entry source_file_entry;
    dir_entry dest_file_entry;
    pathExtruder(sourcefilepath, sourcePathArgs);
    pathExtruder(destfilepath, destPathArgs);

    for (int i = 0; i != sourcePathArgs.size() - 1; i++){
        if(cd(sourcePathArgs[i],true) == -1){
            std::cout << sourcefilepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }

    
    if(!fileExists(sourcePathArgs[sourcePathArgs.size() - 1], source_file_entry)){
        std::cout << "The file " << sourcePathArgs[sourcePathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    if(!accessCheck(source_file_entry.access_rights,READ)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    if(source_file_entry.type == TYPE_DIR){
        std::cout << sourcefilepath << " is a directory (not copied)" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    int numberOfBlocks;
    if(source_file_entry.size % BLOCK_SIZE != 0){
        numberOfBlocks = (source_file_entry.size / BLOCK_SIZE )+1;
    }
    else if(source_file_entry.size == 0){
        numberOfBlocks = 1;
    }
    else{
        numberOfBlocks = source_file_entry.size / BLOCK_SIZE;
    }
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = source_file_entry.first_blk;
    for (int i = 1; i != numberOfBlocks; i++){
        fileBlocks[i] = fat[fileBlocks[i-1]];
    }
    char fileInput[numberOfBlocks][BLOCK_SIZE];
    for(int i = 0; i != numberOfBlocks; i++){
        disk.read(fileBlocks[i],(uint8_t*) & fileInput[i]);
    }
    int* freeBlocks = nullptr;
    if(source_file_entry.size == 0){
        freeBlocks = findFreeBlocks(1);
    }
    else{

        freeBlocks = findFreeBlocks(source_file_entry.size);
    }
    
    if(freeBlocks == nullptr){
        std::cout << "The file can't be copied, no enough space on disk" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    if(destPathArgs.size() > 1){
        for (int i = 0; i != destPathArgs.size() - 1; i++){
            if(cd(destPathArgs[i],true) == -1){
                std::cout << destfilepath << " No such file or directory" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
        }
    }
    
    

    bool destFileFlag = fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry);
    if(!destFileFlag){
        std::string fileName = "";
        

        if(!accessCheck(cwd[0].access_rights,WRITE)){
            std::cout << "Access denied" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }

        if(!entryInit(destPathArgs[destPathArgs.size() - 1],source_file_entry.size,freeBlocks[0],TYPE_FILE)){
            std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
        
        for(int i = 0; i != numberOfBlocks; i++){
            disk.write(freeBlocks[i],(uint8_t*)&fileInput[i]);
        }
        disk.write(FAT_BLOCK,(uint8_t*)&fat);
        
    }
    else
    {
        if(dest_file_entry.type == TYPE_DIR){
            cd(dest_file_entry.file_name,true);
            if(!accessCheck(cwd[0].access_rights,WRITE)){
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            if(!entryInit(sourcePathArgs[sourcePathArgs.size() - 1],source_file_entry.size,freeBlocks[0],TYPE_FILE)){
                std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            
            for(int i = 0; i != numberOfBlocks; i++){
                disk.write(freeBlocks[i],(uint8_t*)&fileInput[i]);
            }
            disk.write(FAT_BLOCK,(uint8_t*)&fat);
        }
        else
        {
            std::string newFileName = destPathArgs[destPathArgs.size() - 1];
            newFileName += " (copied)";
            if(!entryInit(newFileName,source_file_entry.size,freeBlocks[0],TYPE_FILE)){
                std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            
            for(int i = 0; i != numberOfBlocks; i++){
                disk.write(freeBlocks[i],(uint8_t*)&fileInput[i]);
            }
            disk.write(FAT_BLOCK,(uint8_t*)&fat);
        }
        
    }
    
    free(freeBlocks);
    sizeUpdater(source_file_entry.size);
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    int savedBlock = cwdBlock;

    std::vector<std::string> sourcePathArgs;
    std::vector<std::string> destPathArgs;
    dir_entry source_file_entry;
    dir_entry dest_file_entry;
    pathExtruder(sourcepath, sourcePathArgs);
    pathExtruder(destpath, destPathArgs);

    for (int i = 0; i != sourcePathArgs.size() - 1; i++){
        if(cd(sourcePathArgs[i],true) == -1){
            std::cout << sourcepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    
    if(!accessCheck(cwd[0].access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    bool sourceFlag = fileExists(sourcePathArgs[sourcePathArgs.size() - 1], source_file_entry);
    int sourceFileBlock;
    int sourceParentBlock = cwd[0].first_blk; //used for sizeUpdater()
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(cwd[i].file_name == sourcePathArgs[sourcePathArgs.size() - 1]){
            if(!accessCheck(cwd[i].access_rights,READ | WRITE)){
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            sourceFileBlock = cwd[i].first_blk; 
            std::fill_n(cwd[i].file_name, 56, '\0');
            break;
        }
    }
    disk.write(cwdBlock,(uint8_t*)&cwd);

    

    if(!sourceFlag){
        std::cout << "The file or directory " << sourcePathArgs[sourcePathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    else
    {
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        for (int i = 0; i != destPathArgs.size() - 1; i++){
            if(cd(destPathArgs[i],true) == -1){
                std::cout << destpath << " No such file or directory" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
        }
        int destParentBlock = cwd[0].first_blk; //used for sizeUpdater()
        
        
        bool desFlag = fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry);
        
        if ( dest_file_entry.type == TYPE_DIR ){
            cd( dest_file_entry.file_name, true );
            // destParentBlock = cwd[0].first_blk;
            if(!accessCheck(cwd[0].access_rights,WRITE)){
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            if(desFlag){
                dir_entry tmp;
                if(!fileExists(sourcePathArgs[sourcePathArgs.size() - 1], tmp)){
                    if(!entryInit(sourcePathArgs[sourcePathArgs.size() - 1],source_file_entry.size,source_file_entry.first_blk,source_file_entry.type)){
                        std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                }
                else
                {
                    if(!entryInit(tmp.file_name,source_file_entry.size,source_file_entry.first_blk,source_file_entry.type)){
                        std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                }
            }
        }

        else if( dest_file_entry.type == TYPE_FILE )
        {
            if(!accessCheck(cwd[0].access_rights,WRITE)){
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            if(source_file_entry.type == TYPE_DIR){
                std::cout << "can't move a directoy to a file " << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            
            else
            {
                if(desFlag)
                {
                    if(!accessCheck(dest_file_entry.access_rights,WRITE)){
                        std::cout << "Access denied" << std::endl;
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                    int currentBlock = dest_file_entry.first_blk;
                    while (fat[currentBlock] != FAT_EOF ){
                        int tmp = currentBlock;
                        currentBlock = fat[currentBlock];
                        fat[tmp] = FAT_FREE;
                    }
                    fat[currentBlock] = FAT_FREE;
                    disk.write(FAT_BLOCK,(uint8_t*)&fat);

                    std::string fileName = dest_file_entry.file_name;
                    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
                        if(cwd[i].file_name == fileName){
                            std::fill_n(cwd[i].file_name, 56, '\0');
                            break;
                        }
                    }
                    disk.write(cwdBlock,(uint8_t*)&cwd);

                    if(!entryInit(destPathArgs[destPathArgs.size() - 1],source_file_entry.size,source_file_entry.first_blk,source_file_entry.type)){
                        std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                }
                
                else
                {
                     if(!entryInit(sourcePathArgs[sourcePathArgs.size() - 1],source_file_entry.size,source_file_entry.first_blk,source_file_entry.type)){
                        std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                }
                
            } 
        }
        else
        {
            std::cout << "The directory " << dest_file_entry.file_name << " does not exist" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }

        if(source_file_entry.type == TYPE_DIR){
            disk.read(sourceFileBlock,(uint8_t*)&cwd);
            cwd[1].first_blk = dest_file_entry.first_blk;
        }
            disk.read(sourceParentBlock,(uint8_t*)&cwd);
            sizeUpdater(-source_file_entry.size);
            disk.read(destParentBlock,(uint8_t*)&cwd);
            sizeUpdater(source_file_entry.size);

            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return 0;
    }
    
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    int savedBlock = cwdBlock;
    std::vector<std::string> pathArgs;
    pathExtruder(filepath, pathArgs);

    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    if(!accessCheck(cwd[0].access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    dir_entry file_entry;
    if(!fileExists(pathArgs[pathArgs.size() - 1], file_entry)){
        std::cout << "The file " << pathArgs[pathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    if(!accessCheck(file_entry.access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    if(file_entry.type == TYPE_FILE){
        int currentBlock = file_entry.first_blk;
        while (fat[currentBlock] != FAT_EOF ){
            int tmp = currentBlock;
            currentBlock = fat[currentBlock];
            fat[tmp] = FAT_FREE;
        }
        fat[currentBlock] = FAT_FREE;

        disk.write(FAT_BLOCK,(uint8_t*)&fat);

        std::string fileName = file_entry.file_name;
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].file_name == fileName){
                std::fill_n(cwd[i].file_name, 56, '\0');
                break;
            }
        }
        disk.write(cwdBlock,(uint8_t*)&cwd);
    }
    else
    {
        std::cout << filepath << " is a directory!" << std::endl;
        cwdBlock = savedBlock;
        disk.read(cwdBlock,(uint8_t*)&cwd);
        return -1;
    }
    sizeUpdater(-file_entry.size);
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    int savedBlock = cwdBlock;
    std::vector<std::string> sourcePathArgs;
    std::vector<std::string> destPathArgs;
    dir_entry source_file_entry;
    dir_entry dest_file_entry;
    pathExtruder(filepath1, sourcePathArgs);
    pathExtruder(filepath2, destPathArgs);

    for (int i = 0; i != sourcePathArgs.size() - 1; i++){
        if(cd(sourcePathArgs[i],true) == -1){
            std::cout << filepath1 << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    
    if(!fileExists(sourcePathArgs[sourcePathArgs.size() - 1], source_file_entry)){
        std::cout << "The file " << sourcePathArgs[sourcePathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    if(!accessCheck(source_file_entry.access_rights,READ)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    if(source_file_entry.type == TYPE_DIR){
        std::cout << filepath1 << " is a directory (can't be appened)" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    int file1_NumberOfBlocks;
    if(source_file_entry.size % BLOCK_SIZE != 0){
        file1_NumberOfBlocks = (source_file_entry.size / BLOCK_SIZE )+1;
    }
    else if(source_file_entry.size == 0){
        return 0; //There is nothing to append!
    }
    else{
        file1_NumberOfBlocks = source_file_entry.size / BLOCK_SIZE;
    }


    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    for (int i = 0; i != destPathArgs.size() - 1; i++){
        if(cd(destPathArgs[i],true) == -1){
            std::cout << filepath2 << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    
    if(!fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry)){
        std::cout << "The file " << destPathArgs[destPathArgs.size() - 1] << " does not exist "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    if(!accessCheck(dest_file_entry.access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    if(dest_file_entry.type == TYPE_DIR){
        std::cout << filepath2 << "is a directory " << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    int file2_NumberOfBlocks;
    if(dest_file_entry.size % BLOCK_SIZE != 0){
        file2_NumberOfBlocks = (dest_file_entry.size / BLOCK_SIZE )+1;
    }
    else if (dest_file_entry.size == 0){
        file2_NumberOfBlocks = 1;
    }
    else{
        file2_NumberOfBlocks = source_file_entry.size / BLOCK_SIZE;
    }

    int file2_lastBlock = dest_file_entry.first_blk;
    while(fat[file2_lastBlock] != FAT_EOF){
        file2_lastBlock = fat[file2_lastBlock];
    }
    

    int requierdblocks = 0 ;
    int * newBlocks = nullptr;
    if((source_file_entry.size + source_file_entry.size)/BLOCK_SIZE > file1_NumberOfBlocks){
        
        if((source_file_entry.size + source_file_entry.size) % BLOCK_SIZE == 0){
            requierdblocks = ((source_file_entry.size + source_file_entry.size) / BLOCK_SIZE) - file2_NumberOfBlocks;
        }
        else
        {
            requierdblocks = ((source_file_entry.size + dest_file_entry.size) / BLOCK_SIZE) + 1 - file2_NumberOfBlocks;
        }
        
        newBlocks = appendBlocks(dest_file_entry.first_blk,requierdblocks * BLOCK_SIZE);
    }
    char file1_Input[file1_NumberOfBlocks][BLOCK_SIZE];
    int currentBlock = source_file_entry.first_blk;
    int i = 0;
    while(currentBlock != FAT_EOF){
        disk.read(currentBlock,(uint8_t*)&file1_Input[i]);
        currentBlock = fat[currentBlock];
        i++;
    }
    char file2[BLOCK_SIZE];
    disk.read(file2_lastBlock,(uint8_t*)&file2);
    int file2_startIndex = (dest_file_entry.size % BLOCK_SIZE) ;
    int file1_dataIndex = 0;
    
    int file1_blockIndex = 0;
    while(file1_blockIndex != file1_NumberOfBlocks && file2_lastBlock != FAT_EOF ){
        for(int dataIndex = file2_startIndex; dataIndex != BLOCK_SIZE; dataIndex ++){
            file2[dataIndex] = file1_Input[file1_blockIndex][file1_dataIndex];
            if(file1_dataIndex == BLOCK_SIZE - 1){
                file1_dataIndex = 0;
                file1_blockIndex ++;
            }
            file1_dataIndex++;
        }
        file2_startIndex = 0;
        disk.write(file2_lastBlock,(uint8_t*)&file2);
        file2_lastBlock = fat[file2_lastBlock];
    }
    std::string fileName = dest_file_entry.file_name;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(cwd[i].file_name == fileName){
            cwd[i].size += source_file_entry.size;
            break;
        }
    }
    disk.write(cwdBlock,(uint8_t*)&cwd);
    sizeUpdater(source_file_entry.size);
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    int savedBlock = cwdBlock;
    dir_entry parentDir = cwd[0];
    std::vector<std::string> pathArgs;
    pathExtruder(dirpath, pathArgs);


    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << dirpath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    if(!accessCheck(cwd[0].access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    dir_entry file_entry;
    if(fileExists(pathArgs[pathArgs.size() - 1], file_entry)){
        std::cout << "The directory " << pathArgs[pathArgs.size() - 1] << " already exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    int * newBlock = findFreeBlocks(BLOCK_SIZE);
    dir_entry newDir[(BLOCK_SIZE / sizeof(dir_entry)) + 1 ];
    dir_entry self;
    std::fill_n(self.file_name,56,'\0');
    std::fill_n(newDir, (BLOCK_SIZE / sizeof(dir_entry)) + 1, self);
    self.file_name[0] = '.';
    self.first_blk = newBlock[0];
    self.size = 0;
    self.type = TYPE_DIR;
    self.access_rights = WRITE|READ;
    newDir[0] = self;
    
    std::fill_n(parentDir.file_name,56,'\0');
    parentDir.file_name[0] = '.';
    parentDir.file_name[1] = '.';
    parentDir.first_blk = cwd[0].first_blk;
    newDir[1] = parentDir;
    disk.write(newBlock[0],(uint8_t*)&newDir);

    
    if(!entryInit(pathArgs[pathArgs.size() - 1],0,newBlock[0],TYPE_DIR,READ|WRITE)){
        std::cout << "The directory " << pathArgs[pathArgs.size() - 2] << " has no enough space\n";
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>

int
FS::cd(std::string dirpath,bool privateFunc  )
{
    
    if(!privateFunc){
        std::cout << "FS::cd(" << dirpath << ")\n";
    }
    std::vector<std::string> pathArgs;
    pathExtruder(dirpath,pathArgs);

    if(pathArgs[0] == "/"){
        disk.read(ROOT_BLOCK,(uint8_t*)&cwd);
        cwdBlock = ROOT_BLOCK;
        if(pathArgs.size() == 1){
            return 0;
        }
        std::string nextPath = "";
        for (int i = 1; i < pathArgs.size(); i++){
            nextPath += pathArgs[i];
            nextPath += '/';
        }
        nextPath.pop_back();
        std::cout << "next path " << nextPath << "\n";

        if(cd(nextPath, true) == -1){
            if(!privateFunc){
                std::cout << dirpath << " No such file or directory" << std::endl;
            }
            return -1;
        }
    }

    else if(pathArgs[0] == ".."){
        if(cwdBlock != ROOT_BLOCK){
            disk.read(cwd[1].first_blk,(uint8_t*)&cwd);
            cwdBlock = cwd[0].first_blk;
            if(pathArgs.size() == 1){
                return 0;
            }
            std::string nextPath = "";
            for (int i = 1; i < pathArgs.size(); i++){
                nextPath += pathArgs[i];
                nextPath += '/';
            }
            nextPath.pop_back();
            if(cd(nextPath, true) == -1){
                if(!privateFunc){
                    std::cout << dirpath << " No such file or directory" << std::endl;
                }
                return -1;
            }
        }
        else
        {
            if(pathArgs.size() == 1){
                return 0;
            }
            std::string nextPath = "";
            for (int i = 1; i < pathArgs.size(); i++){
                nextPath += pathArgs[i];
                nextPath += '/';
            }
            nextPath.pop_back();
            if(cd(nextPath, true) == -1){
                if(!privateFunc){
                    std::cout << dirpath << " No such file or directory" << std::endl;
                }
            return -1;
            }
        }
    }

    else
    {
        dir_entry dirEntry;

        if(!fileExists(pathArgs[0],dirEntry)){
            if(!privateFunc){
                std::cout << dirpath << " No such file or directory" << std::endl;
            }
            return -1;
        }
        else
        {
            if(dirEntry.type != TYPE_FILE){
                disk.read(dirEntry.first_blk,(uint8_t*)&cwd);
                cwdBlock = dirEntry.first_blk;
                if(pathArgs.size() == 1){
                    return 0;
                }
                std::string nextPath = "";
                for (int i = 1; i < pathArgs.size(); i++){
                    nextPath += pathArgs[i];
                    nextPath += '/';
                }
                nextPath.pop_back();

                if(cd(nextPath, true) == -1){
                    if(!privateFunc){
                        std::cout << dirpath << " No such file or directory" << std::endl;
                    }
                    return -1;
                }
            }
            else
            {
                if(!privateFunc){
                    std::cout << dirpath <<": Not a directory" << std::endl;
                }
                return -1;
            }
        }
    }
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    int savedBlock = cwdBlock;
    std::string fullPath = "";
    int childBlock = cwd[0].first_blk;;
    while(childBlock != ROOT_BLOCK ){
        std::string temp = "";
        cd("..", true);
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].first_blk == childBlock){
                temp += cwd[i].file_name;
                temp += '/';
                temp += fullPath;
                fullPath = temp;
                childBlock = cwd[0].first_blk;
                break;
            }
        }
    }
    fullPath.pop_back();
    std::cout << '/' + fullPath << std::endl;

    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    int savedBlock = cwdBlock;
    std::string accessValuesString[7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};
    std::vector<std::string> pathArgs;
    pathExtruder(filepath,pathArgs);
    for(int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i]) == -1){
            std::cout << filepath << ": No such file or directory" << std::endl;
            cwdBlock = savedBlock;
            disk.read(savedBlock,(uint8_t*)&cwd);
            return -1;
        }
    }
    dir_entry fileEntry;
    if(!fileExists(pathArgs[pathArgs.size() - 1],fileEntry)){
        std::cout << filepath << ": No such file or directory" << std::endl;
            cwdBlock = savedBlock;
            disk.read(savedBlock,(uint8_t*)&cwd);
        return -1;
    }
    else{
        std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
        std::string fileName = "";
        fileName += fileEntry.file_name;
        bool formatFound = false;
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i++){
            if(cwd[i].file_name == fileName){
                for(int j = 0; j != 7; j++ ){
                    if(accessrights == accessValuesString[j]){
                        cwd[i].access_rights = j + 1;
                        disk.write(cwdBlock,(uint8_t*)&cwd);
                        if(fileEntry.type == TYPE_DIR){
                            cd(fileEntry.file_name, true);
                            cwd[0].access_rights = j + 1;
                            disk.write(cwdBlock,(uint8_t*)&cwd);
                            cd("..", true);
                        }
                        formatFound = true;
                        break;
                    }
                }
                if(!formatFound){
                    std::cout << "Invalid format, available formats: (--e, -w-, -we, r--, r-e, rw-, rwe) " << std::endl;
                }

                break;
                
            }
        }
    }
    cwdBlock = savedBlock;
    disk.read(savedBlock,(uint8_t*)&cwd);
    return 0;
}
