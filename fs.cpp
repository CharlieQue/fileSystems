#include <iostream>
#include <sstream>
#include "fs.h"

/*
A constructor to read the current blocks from the disk
*/
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

/* A func to put the std::string path pased to the other functions
into an dynamkc array of directories names*/
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

/* Func to find the available free blocks on the disk 
based on the fat values*/
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
        return nullptr; //if no enough space on the disk, this helps the main functions to check if there is enough space
    }

    /* fill the fat table with the needed blocks*/
    for(int i = 0; i != numOfBlocks; i ++){
        if(i != numOfBlocks - 1 ){
            fat[freeBlocks[i]] = freeBlocks[i+1];
        }
        
    }

    return freeBlocks;
}

/* Func to append more blocks to a file if needed*/
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

/* Func to check if a file or directory exist in the curren working directory*/
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

/*Func to add a new directoy entry and its information to the current working directory */
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

    /* this loop checks if there is enough space in the current working directory for the new directory entry*/
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

/*Func to check if we have a specific access to make som actions for a file or directory */
bool 
FS::accessCheck(uint8_t access_rights, uint8_t requestedAccess){
    std::string accessValues[7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};
    std::string requestedAccessString = accessValues[requestedAccess - 1];
    std::string accessValuesString = accessValues[access_rights - 1];
    for(int i = 0; i < 3; i++){
        if (requestedAccessString[i] != '-'){
            if(requestedAccessString[i] != accessValuesString[i]){
                return false; //return false if we don't have the access i.g. file_access_rights r-- and reqiuested access: -w- will return false!
            }
        }
    }
    return true;
}

/*Func to update the size for each directory till it rach the root directory */
void
FS::sizeUpdater(size_t sizeArgs){
    int savedBlock = cwdBlock;
    uint16_t childBlock = cwd[0].first_blk; // To find the recently updated child directoy entry in the parent directory
    cwd[0].size += sizeArgs;
    disk.write(cwd[0].first_blk,(uint8_t*)&cwd);
    while(childBlock != ROOT_BLOCK ){   //then we reach the root directory 
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
    /* loop sets all disk block in the fat table to free block i.e 0 exept the root directory and the fat blocks*/
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
    /*A . directoy which have information about the directory itself*/
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
    int savedBlock = cwdBlock; //to map to the cwd after cd()'s calls
    std::vector<std::string> pathArgs;
    pathExtruder(filepath, pathArgs); //to get evry directoy name in an array 

    /* this loop maps to the requested path 
    and checks if the path is approachable or not*/
    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    // checks if it is allowed to create a file in the requested path
    if(!accessCheck(cwd[0].access_rights,WRITE)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    dir_entry file_entry;
    if(fileExists(pathArgs[pathArgs.size() - 1], file_entry)){ //Checks if the file is already existed
        std::cout << "The file " << pathArgs[pathArgs.size() - 1] << " already exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    /* ######################### 
    To get data input from the user
    */
    std::string line;
    char c;
    std::getline(std::cin, line);
    std::stringstream linestream(line);
    char fileOtput[BLOCK_SIZE];

    /*############################
    */

    std::fill_n(fileOtput,BLOCK_SIZE, '\0');
    int* freeBlocks = findFreeBlocks(line.size());
    if(freeBlocks == nullptr){ //checks if there is enough space on disk
        std::cout << "The file can't be created, no enough space on disk" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    //checks if it is possiable to create a file entry in the requested path
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
        //writing the data to the disk
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
    
    //updating the fat table
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

    /*loop and check if the directory path is approachable*/
    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    dir_entry file_entry;
    if(!fileExists(pathArgs[pathArgs.size() - 1], file_entry)){ //chacks if the file exsist in the path directory
        std::cout << "The file " << pathArgs[pathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    //check if the process is allowed to read from the requested file
    if(!accessCheck(file_entry.access_rights,READ)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    if(file_entry.type == TYPE_DIR){//not allowed to prints out directories
        std::cout << filepath <<" is a directory." << std::endl;
        return -1;
    }
    /* if the file's size is 0 so there is no 
    need go furthur in the process. just print and empty line
    */
    if(file_entry.size == 0){
        std::cout << std::endl;
        return 0;
    }
    int numberOfBlocks; //variable to store the number of blocks the file requires
    if(! (file_entry.size % BLOCK_SIZE == 0)){
        numberOfBlocks = (file_entry.size / BLOCK_SIZE )+1;
    }
    else
    {
        numberOfBlocks = file_entry.size / BLOCK_SIZE;
    }
    /*This array stored which disk's block 
    the disk.read() func should read from.*/
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = file_entry.first_blk;
    //loop to store the file disk's blocks based on the FAT 
    for (int i = 1; i != numberOfBlocks; i++)
    {
        fileBlocks[i] = fat[fileBlocks[i-1]];
    }

    /*loop to read the disk's blocks and prints out the data*/
    for(int i = 0; i != numberOfBlocks; i++){
        char fileInput[BLOCK_SIZE];
        disk.read(fileBlocks[i],(uint8_t*) & fileInput);
        std::cout << fileInput << std::flush;
    }
    std::cout << std::endl;
    /*maps to the working directoy that the cat command has been called from*/
    disk.read(savedBlock,(uint8_t*)&cwd);
    cwdBlock = savedBlock;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
    /*An array to store the access rights as a strings*/
    std::string accessArray [7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};

    std::cout << std::setw(25) << std::left << "Filename" << std::setw(25) << std::left  <<  "Type" << std::setw(25) << std::left << "Access rights" << std::setw(25) << std::left << "Size" << std::endl;
    //loops through the whole current working directory
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
    /* A variable to store the current working 
    directory block. Used to maps again to the 
    same directoy after the cd() func calls. */
    int savedBlock = cwdBlock; 

    std::vector<std::string> sourcePathArgs;
    std::vector<std::string> destPathArgs;
    dir_entry source_file_entry; //A variable to store the information about the source file directory entry
    dir_entry dest_file_entry;//A variable to store the information about the destination file directory entry
    pathExtruder(sourcefilepath, sourcePathArgs);
    pathExtruder(destfilepath, destPathArgs);

    /* First map to the source file parent directory*/
    for (int i = 0; i != sourcePathArgs.size() - 1; i++){
        if(cd(sourcePathArgs[i],true) == -1){
            std::cout << sourcefilepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }

    /* Check if the file actually exists*/
    if(!fileExists(sourcePathArgs[sourcePathArgs.size() - 1], source_file_entry)){
        std::cout << "The file " << sourcePathArgs[sourcePathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    /* Check if the file have a read access */
    if(!accessCheck(source_file_entry.access_rights,READ)){
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    /* if the file is a directory, can't be copied */
    if(source_file_entry.type == TYPE_DIR){
        std::cout << sourcefilepath << " is a directory (not copied)" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }

    /*numberOfBlocks stores how many blocks the source file
    require to be copied*/
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
    /*This array stored which disk's blocks 
    the disk.read() func should read from.*/
    int fileBlocks[numberOfBlocks];
    fileBlocks[0] = source_file_entry.first_blk;
    for (int i = 1; i != numberOfBlocks; i++){
        fileBlocks[i] = fat[fileBlocks[i-1]];
    }
    /*fileInput stores the source file data*/
    char fileInput[numberOfBlocks][BLOCK_SIZE];
    for(int i = 0; i != numberOfBlocks; i++){
        disk.read(fileBlocks[i],(uint8_t*) & fileInput[i]);//put the source file data in the fileInput array
    }

    int* freeBlocks = nullptr;
    if(source_file_entry.size == 0){
        freeBlocks = findFreeBlocks(1);//calls findFreeBlocks() and checks if the there is eough space on the disk
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
    
    /*if the destination path is only one directory
    then the source file should be copied to the currend
    working directory with the destination path name.
    Note: if the destination path exists in the current
    working directory and it is a directory then the source
    file get copied into this directory */

    bool destFileFlag = fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry);
    if(!destFileFlag){//if the given destination file name does not exist
        std::string fileName = destPathArgs[destPathArgs.size() - 1];
        
        if(destPathArgs.size() == 1 && destPathArgs[0] == "/"){//special case when we want to copy a file to the root directory
            fileName = sourcePathArgs[sourcePathArgs.size() - 1];
        }

        if(!accessCheck(cwd[0].access_rights,WRITE)){//checks if the working directory have write access right
            std::cout << "Access denied" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
        dir_entry temp;
        if(fileExists(fileName, temp)){//chack if the file name already exists 
            fileName += " (copied)";
        }
        //add the file directoy entry into the destination directoy
        if(!entryInit(fileName,source_file_entry.size,freeBlocks[0],TYPE_FILE)){
            std::cout << "The directory " << destPathArgs[destPathArgs.size() - 2] << " has no enough space\n";
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
        //write the file data to the disk
        for(int i = 0; i != numberOfBlocks; i++){
            disk.write(freeBlocks[i],(uint8_t*)&fileInput[i]);
        }
        disk.write(FAT_BLOCK,(uint8_t*)&fat);
        
    }
    else
    {
        if(dest_file_entry.type == TYPE_DIR){//if the destination file is an directory, map into the directory
            cd(dest_file_entry.file_name,true);
            if(!accessCheck(cwd[0].access_rights,WRITE)){//chack the access right on write
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            //add the file directoy entry into the destination directoy
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
            //add the file directoy entry into the destination directoy as a copy virsion
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

    //first map into the source parent directory
    if(sourcePathArgs.size() > 1){
        for (int i = 0; i != sourcePathArgs.size() - 1; i++){
            if(cd(sourcePathArgs[i],true) == -1){
                std::cout << sourcepath << " No such file or directory" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
        }
    }
    //checks if the process is allowed to edit in the parent directory
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
            if(!accessCheck(cwd[i].access_rights,READ | WRITE)){//checks the rights of the target file or directoy 
                std::cout << "Access denied" << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            sourceFileBlock = cwd[i].first_blk; 
            std::fill_n(cwd[i].file_name, 56, '\0');//removes the directory entry from the parent directory.
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
        disk.read(sourceParentBlock,(uint8_t*)&cwd);
        sizeUpdater(-source_file_entry.size);//update the size for the source parent directoy

        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        if(destPathArgs.size() > 1){//maps to the destination file parent directory
            for (int i = 0; i != destPathArgs.size() - 1; i++){
                if(cd(destPathArgs[i],true) == -1){
                    std::cout << destpath << " No such file or directory" << std::endl;
                    disk.read(savedBlock,(uint8_t*)&cwd);
                    cwdBlock = savedBlock;
                    return -1;
                }
            }
        }
        
        int destParentBlock = cwd[0].first_blk; //used for sizeUpdater()
        
        
        bool desFlag = fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry);
        //if the destination target is a directoy so we move the source tagret into this directory
        if ( dest_file_entry.type == TYPE_DIR ){
            cd( dest_file_entry.file_name, true );
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
            if(source_file_entry.type == TYPE_DIR){//if we tried to move a directory to a file
                std::cout << "can't move a directoy to a file " << std::endl;
                disk.read(savedBlock,(uint8_t*)&cwd);
                cwdBlock = savedBlock;
                return -1;
            }
            
            else //moving file to a file
            {
                if(desFlag)
                {
                    if(!accessCheck(dest_file_entry.access_rights,WRITE)){
                        std::cout << "Access denied" << std::endl;
                        disk.read(savedBlock,(uint8_t*)&cwd);
                        cwdBlock = savedBlock;
                        return -1;
                    }
                    int currentBlock = source_file_entry.first_blk;
                    std::cout << "check 1" << std::endl;
                    while (fat[currentBlock] != FAT_EOF ){
                        int tmp = currentBlock;
                        currentBlock = fat[currentBlock];
                        fat[tmp] = FAT_FREE;
                    }
                    fat[currentBlock] = FAT_FREE;
                    disk.write(FAT_BLOCK,(uint8_t*)&fat);
                    std::cout << "check 2" << std::endl;
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
        
        disk.read(destParentBlock,(uint8_t*)&cwd);
        sizeUpdater(source_file_entry.size);
        std::cout << "Check 5" << std::endl;
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

    //maps to the given path
    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << filepath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }

    if(!accessCheck(cwd[0].access_rights,WRITE)){//checks if it is allowed to do changes on the parent directory
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
    if(!accessCheck(file_entry.access_rights,WRITE)){//checks if it's allowed to remove the target file
        std::cout << "Access denied" << std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    
    if(file_entry.type == TYPE_FILE){
        //currentBlock holds the value of the starting block of the taret file
        int currentBlock = file_entry.first_blk;

        //free the file's FAT blocks
        while (fat[currentBlock] != FAT_EOF ){
            int tmp = currentBlock;
            currentBlock = fat[currentBlock];
            fat[tmp] = FAT_FREE;
        }
        fat[currentBlock] = FAT_FREE;

        disk.write(FAT_BLOCK,(uint8_t*)&fat);

        std::string fileName = file_entry.file_name;
        //this loop remove the file directory entry from the tparent directory
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].file_name == fileName){
                std::fill_n(cwd[i].file_name, 56, '\0');
                break;
            }
        }
        disk.write(cwdBlock,(uint8_t*)&cwd);
    }
    else //not allowed to remove a directory
    {
        std::cout << filepath << " is a directory!" << std::endl;
        cwdBlock = savedBlock;
        disk.read(cwdBlock,(uint8_t*)&cwd);
        return -1;
    }
    sizeUpdater(-file_entry.size); //udate the size of the parent directory
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

    //map to the parent directory of the source file
    for (int i = 0; i != sourcePathArgs.size() - 1; i++){
        if(cd(sourcePathArgs[i],true) == -1){
            std::cout << filepath1 << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    //checks if the file exists
    if(!fileExists(sourcePathArgs[sourcePathArgs.size() - 1], source_file_entry)){
        std::cout << "The file " << sourcePathArgs[sourcePathArgs.size() - 1] << " does not exists "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    //check if the process is allowed to read information from the source file
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

    //file1_NumberOfBlocks holds the number of blocks the source file require to be appended
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
    for (int i = 0; i != destPathArgs.size() - 1; i++){//map to the destination target
        if(cd(destPathArgs[i],true) == -1){
            std::cout << filepath2 << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    //check if the destination file exists
    if(!fileExists(destPathArgs[destPathArgs.size() - 1], dest_file_entry)){
        std::cout << "The file " << destPathArgs[destPathArgs.size() - 1] << " does not exist "<< std::endl;
        disk.read(savedBlock,(uint8_t*)&cwd);
        cwdBlock = savedBlock;
        return -1;
    }
    //checks if the process can write in the destination file
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

    /*file2_lastBlock will have the value 
    of the destination last disk block. Helps to know where
    to start appending the data*/
    int file2_lastBlock = dest_file_entry.first_blk;
    while(fat[file2_lastBlock] != FAT_EOF){
        file2_lastBlock = fat[file2_lastBlock];
    }
    

    int requierdblocks = 0 ; //How many blocks the source file require to be appended
    int * newBlocks = nullptr;
    if((source_file_entry.size + source_file_entry.size)/BLOCK_SIZE > file1_NumberOfBlocks){
        
        if((source_file_entry.size + source_file_entry.size) % BLOCK_SIZE == 0){
            requierdblocks = ((source_file_entry.size + source_file_entry.size) / BLOCK_SIZE) - file2_NumberOfBlocks;
        }
        else
        {
            requierdblocks = ((source_file_entry.size + dest_file_entry.size) / BLOCK_SIZE) + 1 - file2_NumberOfBlocks;
        }
        //append the required blocks to the destination file
        newBlocks = appendBlocks(dest_file_entry.first_blk,requierdblocks * BLOCK_SIZE);
    }

    char file1_Input[file1_NumberOfBlocks][BLOCK_SIZE];
    int currentBlock = source_file_entry.first_blk;
    int i = 0;
    while(currentBlock != FAT_EOF){
        // put the source file data into the file1_Input array
        disk.read(currentBlock,(uint8_t*)&file1_Input[i]);
        currentBlock = fat[currentBlock];
        i++;
    }
    char file2[BLOCK_SIZE];
    disk.read(file2_lastBlock,(uint8_t*)&file2);
    int file2_startIndex = (dest_file_entry.size % BLOCK_SIZE); //where to start appending the data
    int file1_dataIndex = 0;
    
    int file1_blockIndex = 0;
    //append until we reach the end of the source file.
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

    //map to the parent directory 
    for (int i = 0; i != pathArgs.size() - 1; i++){
        if(cd(pathArgs[i],true) == -1){
            std::cout << dirpath << " No such file or directory" << std::endl;
            disk.read(savedBlock,(uint8_t*)&cwd);
            cwdBlock = savedBlock;
            return -1;
        }
    }
    //Checks if the process can do changes in the parent directory
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
    //Add the . directory entry which stores information about the directory that will be created
    dir_entry self;
    std::fill_n(self.file_name,56,'\0');
    std::fill_n(newDir, (BLOCK_SIZE / sizeof(dir_entry)) + 1, self);
    self.file_name[0] = '.';
    self.first_blk = newBlock[0];
    self.size = 0;
    self.type = TYPE_DIR;
    self.access_rights = WRITE|READ;
    newDir[0] = self;
    // Add the .. directory entry which stores information about the parent directory
    std::fill_n(parentDir.file_name,56,'\0');
    parentDir.file_name[0] = '.';
    parentDir.file_name[1] = '.';
    parentDir.first_blk = cwd[0].first_blk;
    newDir[1] = parentDir;
    disk.write(newBlock[0],(uint8_t*)&newDir);

    //append the new directoy entry to the parent
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

    //if the path is / then the process load the root directory to the cwd array
    if(pathArgs[0] == "/"){
        disk.read(ROOT_BLOCK,(uint8_t*)&cwd);
        cwdBlock = ROOT_BLOCK;
        if(pathArgs.size() == 1){
            return 0;
        }
        std::string nextPath = "";
        for (int i = 1; i < pathArgs.size(); i++){//fill the next path from the pathArgs array and recursive call cd() again
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
    //if the target is .. the load the parent directory and recursivly call cd on the remaining path
    else if(pathArgs[0] == ".."){
        if(cwdBlock != ROOT_BLOCK){ //if the process ing the root directory, just call the remaining path
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
        //find the first block of the target directory by call fileExists()
        dir_entry dirEntry;
        if(!fileExists(pathArgs[0],dirEntry)){
            if(!privateFunc){
                std::cout << dirpath << " No such file or directory" << std::endl;
            }
            return -1;
        }
        else
        {
            /* if the target is file, just stay in the directory. 
            else load the cwd array and call recursivly cd() on 
            the remaining path */
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
    int childBlock = cwd[0].first_blk;
    /* while we the process not reached its final destination
    load the parent directory and add the name of the child directory
    i.e the directory that the process has came from*/
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
    //Array to store all possible mode values
    std::string accessValuesString[7] = {"--e", "-w-", "-we", "r--", "r-e", "rw-", "rwe"};
    
    std::vector<std::string> pathArgs;
    pathExtruder(filepath,pathArgs);
    /*maps to the target parent directory. and checks if the path is approachable*/
    for(int i = 0; i != pathArgs.size() - 1; i++){ 
        if(cd(pathArgs[i]) == -1){
            std::cout << filepath << ": No such file or directory" << std::endl;
            cwdBlock = savedBlock;
            disk.read(savedBlock,(uint8_t*)&cwd);
            return -1;
        }
    }
    dir_entry fileEntry;
    //checks if the target directory entry exists
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
        //first loop till the process find the file entry
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i++){
            if(cwd[i].file_name == fileName){
                for(int j = 0; j != 7; j++ ){ //loop through the available access strings
                    if(accessrights == accessValuesString[j]){//if the given value is allowed
                        cwd[i].access_rights = j + 1; //change teh access rights for the file entry in the parent directroy
                        disk.write(cwdBlock,(uint8_t*)&cwd);
                        if(fileEntry.type == TYPE_DIR){
                            cd(fileEntry.file_name, true);
                            cwd[0].access_rights = j + 1;//change teh access rights for the file entry in the target directroy
                            disk.write(cwdBlock,(uint8_t*)&cwd);
                            cd("..", true);
                        }
                        formatFound = true; //to mark that the given value is allowed.
                        break;
                    }
                }
                if(!formatFound){ //if the given value not allowed
                    std::cout << "Invalid file mode, available modes: (--e, -w-, -we, r--, r-e, rw-, rwe) " << std::endl;
                }

                break;
                
            }
        }
    }
    cwdBlock = savedBlock;
    disk.read(savedBlock,(uint8_t*)&cwd);
    return 0;
}
