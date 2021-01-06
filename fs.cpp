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
FS::fileExists(uint16_t & firstBlock, uint32_t & fileSize, int & numberOfBlocks,std::string filepath,int & type ){
    bool seperatorFound = false;
    int lastSeperatorIndex;
    for(int i = 0; i != filepath.length(); i++){
        if(filepath[i] == '/'){
            seperatorFound = true;
            lastSeperatorIndex = i;
        }
    }
    if(seperatorFound){
        cd(filepath,true);
        std::string newPath = "";
        newPath.append(&filepath[lastSeperatorIndex +1]);
        if(fileExists(firstBlock,fileSize,numberOfBlocks,newPath,type)){
            return true;
        }
    }
    bool fileExists = false;
    for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
        if(cwd[i].file_name == filepath){
            fileExists = true;
            firstBlock = cwd[i].first_blk;
            fileSize = cwd[i].size;
            type = cwd[i].type;
            break;
        }
    }
    if(!fileExists){
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
FS::entryInit(std::string filepath, size_t fileSize, uint16_t firstBlock, int type,int access_rights){
    bool seperatorFound = false;
    int lastSeperatorIndex;
    for(int i = 0; i != filepath.length(); i++){
        if(filepath[i] == '/'){
            seperatorFound = true;
            lastSeperatorIndex = i;
        }
    }
    std::string fileName = "";
    if(seperatorFound){
        cd(filepath,true);
        fileName.append(&filepath[lastSeperatorIndex + 1]);
    }
    else{
        fileName = filepath;
    }
    
    dir_entry newEntry;
    std::fill_n(newEntry.file_name, 56, '\0');
    for(int i = 0; i != fileName.length(); i ++){
        newEntry.file_name[i]= fileName[i];
    }
    newEntry.first_blk = firstBlock;
    newEntry.size = fileSize;
    newEntry.type = type;
    newEntry.access_rights = READ|WRITE;
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
        else {
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
    int savedBlock = cwdBlock;
    uint16_t optional1;
    uint32_t optional2;
    int optional3;
    if(fileExists(optional1,optional2,optional3,filepath,optional3)){
        std::cout << "the file " << filepath << " does exist" << std::endl;
        return -1;
    }
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
    
    entryInit(filepath,line.length(),freeBlocks[0],TYPE_FILE);
    free(freeBlocks);
    while(cwdBlock != ROOT_BLOCK){
        cwd[0].size += line.size();
        disk.write(cwdBlock,(uint8_t*)&cwd);
        cd("..",true);
    }
    
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
    uint16_t firsBlock;
    uint32_t fileSize;
    int numberOfBlocks;
    int type;
    if(!fileExists(firsBlock,fileSize,numberOfBlocks,filepath,type)){
        std::cout << "The file " << filepath << " do not exist!" << std::endl;
        return -1;
    }
    if(type == TYPE_DIR){
        std::cout << filepath <<" is a directory." << std::endl;
        return -1;
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
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
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
    int savedBlock = cwdBlock;
    uint16_t firsBlock;
    uint32_t fileSize;
    int numberOfBlocks;
    int type;
    if(!fileExists(firsBlock,fileSize,numberOfBlocks,sourcefilepath,type)){
        std::cout << "The file " << sourcefilepath << " do not exist!" << std::endl;
        return -1;
    }
    if(type == TYPE_DIR){
        std::cout << sourcefilepath << "is a directory (not copied)" << std::endl;
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
    entryInit(destfilepath,fileSize,firsBlock,TYPE_FILE);
    free(freeBlocks);
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    int savedBlock = cwdBlock;
    uint16_t sourceFileFirsBlock , destFileFirsBlock ;
    uint32_t sourceFileSize , destFileSize;
    int sourceFileNumberOfBlocks , destFileNumberOfBlocks;
    int sourceFileType , destFileType;
    if(!fileExists(sourceFileFirsBlock,sourceFileSize,sourceFileNumberOfBlocks,sourcepath,sourceFileType)){
        std::cout << "The file " << sourcepath << " do not exist!" << std::endl;
        return -1;
    }
    int sourceFileWD = cwdBlock;
    if(fileExists(destFileFirsBlock,destFileSize,destFileNumberOfBlocks,destpath,destFileType)){
        if(destFileType == TYPE_FILE){
            std::cout<< "The file " << destpath << " already exists, not allowed to have two files with the same name in the same directory." << std::endl;
            return -1;
        }
    }
    int destFileWD = cwdBlock;
    bool condition1 = (sourceFileType == TYPE_FILE && destFileType == TYPE_FILE);
    bool condition2 = (sourceFileType == TYPE_FILE && destFileType == TYPE_DIR);
    if(condition1|| condition2){
        disk.read(sourceFileWD,(uint8_t*)&cwd); 
        dir_entry currentEntry;
        int seperatorIndex = 0;
        std::string fileName = "";
        if(sourcepath[0] == '.' || sourcepath[0] == '/'){
            for(int i = 0; i != sourcepath.length(); i++){
                if(sourcepath[i] == '/'){
                    seperatorIndex = i;
                }
            }
            fileName.append(&sourcepath[seperatorIndex +1]);
        }
        else{
            fileName = sourcepath;
        }
        
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].file_name == fileName){
                currentEntry = cwd[i];
                dir_entry temp;
                cwd[i] = temp;
                if(condition1){
                    entryInit(destpath,sourceFileSize,sourceFileFirsBlock,TYPE_FILE,currentEntry.access_rights);
                }
                else if(condition2){ 
                    std::string finalPath = destpath + '/' + fileName;
                    entryInit(finalPath,sourceFileSize,sourceFileFirsBlock,TYPE_FILE,currentEntry.access_rights);
                }
                
            }
        }
    }
    else
    {
        disk.read(sourceFileWD,(uint8_t*)&cwd);
        dir_entry myDir = cwd[0];
        disk.read(cwd[1].first_blk,(uint8_t*)&cwd);
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].file_name == myDir.file_name){
                dir_entry temp;
                cwd[i] = temp;
                break;
            }
        }
        disk.read(sourceFileWD,(uint8_t*)&cwd);
        cwd[1].first_blk = destFileWD;
        std::string finalPath = destpath + '/' + myDir.file_name;
        entryInit(finalPath,sourceFileSize,sourceFileFirsBlock,TYPE_DIR,myDir.access_rights);
    }
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    int savedBlock = cwdBlock;
    std::cout << "FS::rm(" << filepath << ")\n";
    uint16_t firsBlock;
    uint32_t fileSize;
    int numberOfBlocks;
    int type;
    if(!fileExists(firsBlock,fileSize,numberOfBlocks,filepath,type)){
        std::cout << "The file " << filepath << " does not exist!" << std::endl;
        return -1;
    }
    if(type == TYPE_FILE){
        int currentBlock = firsBlock;
        while (fat[currentBlock] != FAT_EOF ){
            int tmp = currentBlock;
            currentBlock = fat[currentBlock];
            fat[tmp] = FAT_FREE;
        }
        fat[currentBlock] = FAT_FREE;
        disk.write(FAT_BLOCK,(uint8_t*)&fat);
        int seperatorIndex = 0;
        std::string fileName = "";
        if(filepath[0] == '.' || filepath[0] == '/'){
            for(int i = 0; i != filepath.length(); i++){
                if(filepath[i] == '/'){
                    seperatorIndex = i;
                }
            }
            fileName.append(&filepath[seperatorIndex +1]);
        }
        else{
            fileName = filepath;
        }

        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
                if(cwd[i].file_name == fileName){
                    dir_entry temp;
                    cwd[i] = temp;
                    break;
                }
            }
        
        disk.write(cwdBlock,(uint8_t*)&cwd);
    }
    else{
        std::cout << filepath << " is a directory!" << std::endl;
    }
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    int savedBlock = cwdBlock;
    uint16_t file1_FirstBlock;
    uint32_t file1_FileSize;
    int file1_NumberOfBlocks;
    int type;
    if(!fileExists(file1_FirstBlock,file1_FileSize,file1_NumberOfBlocks,filepath1,type)){
        std::cout << "The file " << filepath1 << " do not exist!" << std::endl;
        return -1;
    }
    uint16_t file2_FirstBlock;
    uint32_t file2_FileSize;
    int file2_NumberOfBlocks;
    if(!fileExists(file2_FirstBlock,file2_FileSize,file2_NumberOfBlocks,filepath2,type)){
        std::cout << "The file "  << filepath2 << " do not exist!" << std::endl;
        return -1;
    } 
    int file2_lastBlock = file2_FirstBlock;
    while(fat[file2_lastBlock] != FAT_EOF){
        file2_lastBlock = fat[file2_lastBlock];
    }
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    int requierdblocks = 0 ;
    int * newBlocks = nullptr;
    if((file1_FileSize + file1_FileSize)/BLOCK_SIZE > file1_NumberOfBlocks){
        
        if((file1_FileSize + file2_FileSize) % BLOCK_SIZE == 0){
            requierdblocks = ((file1_FileSize + file2_FileSize) / BLOCK_SIZE) - file2_NumberOfBlocks;
        }
        else
        {
            requierdblocks = ((file1_FileSize + file2_FileSize) / BLOCK_SIZE) + 1 - file2_NumberOfBlocks;
        }
        
        newBlocks = appendBlocks(file2_FirstBlock,requierdblocks * BLOCK_SIZE);
    }
    char file1_Input[file1_NumberOfBlocks][BLOCK_SIZE];
    int currentBlock = file1_FirstBlock;
    int i = 0;
    while(currentBlock != FAT_EOF){
        disk.read(currentBlock,(uint8_t*)&file1_Input[i]);
        currentBlock = fat[currentBlock];
        i++;
    }
    char file2[BLOCK_SIZE];
    disk.read(file2_lastBlock,(uint8_t*)&file2);
    int file2_startIndex = (file2_FileSize % BLOCK_SIZE) ;
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
        std::cout << file2_lastBlock << std::endl;
        disk.write(file2_lastBlock,(uint8_t*)&file2);
        file2_lastBlock = fat[file2_lastBlock];

    }
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    int savedBlock = cwdBlock;
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    uint16_t optional1;
    uint32_t optional2;
    int optional3;
    int type;
    if(fileExists(optional1,optional2,optional3,dirpath,type)){
        if(type == TYPE_DIR){
            std::cout << "The directory " << dirpath << " does exist "<< std::endl;
        }
        else{
            std::cout << "A file with the name " << dirpath << " alrady exists, try another name" << std::endl;
        }
        return -1;
    }
    else{
        int * newBlock = findFreeBlocks(BLOCK_SIZE);
        dir_entry newDir[(BLOCK_SIZE / sizeof(dir_entry))+1];
        dir_entry self;
        std::fill_n(self.file_name,56,'\0');
        std::fill_n(newDir,(BLOCK_SIZE/sizeof(dir_entry)) + 1,self);
        self.file_name[0] = '.';
        self.first_blk = newBlock[0];
        self.size = 0;
        self.type = TYPE_DIR;
        self.access_rights = WRITE|READ;
        newDir[0] = self;
        dir_entry parentDir = cwd[0];
        std::fill_n(parentDir.file_name,56,'\0');
        parentDir.file_name[0] = '.';
        parentDir.file_name[1] = '.';
        newDir[1] = parentDir;
        disk.write(newBlock[0],(uint8_t*)&newDir);
        entryInit(dirpath,0,newBlock[0],TYPE_DIR,READ|WRITE);
        
    }
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath,bool privateFunc  ){
    
    if(!privateFunc){
        std::cout << "FS::cd(" << dirpath << ")\n";
    }
    if(dirpath.length() == 0){
        return 0;
    }
    unsigned int dirCounter = 1;
    std::string myArr [dirpath.length()];
    myArr[0] = dirpath[0];

    int dirPathIndex = 1;
    int myArrIndex = 0;
    if(dirpath[0] == '/'){
        myArrIndex = 1;
        if(dirCounter != dirpath.length()){
            dirCounter++;
        }
    }
    while (dirPathIndex < dirpath.length()) {
        if(dirpath[dirPathIndex] != '/'){
            myArr[myArrIndex] += dirpath[dirPathIndex];
        }
        else{
            myArrIndex++;
            dirCounter++;
        }
        dirPathIndex++;
    }
    if(myArr[0] == "/"){
        disk.read(ROOT_BLOCK,(uint8_t*)&cwd);
        cwdBlock = ROOT_BLOCK;

        std::string nextPath = "";
        if(dirCounter != dirpath.length()){
            for (int i = 1; i < dirCounter ; i++){
                nextPath += myArr[i];
                nextPath += '/';
            }
        }
        nextPath.pop_back();
        if(cd(nextPath, true) == -1){
            if(!privateFunc){
                std::cout << dirpath << " No such file or directory" << std::endl;
            }
        }
    }
    else
    {
        uint16_t firstBlock;
        uint8_t type;
        bool fileExists = false;
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].file_name == myArr[0]){          
                fileExists = true;
                firstBlock = cwd[i].first_blk;
                type = cwd[i].type;
                break;
            }
        }
        if(!fileExists){
            if(!privateFunc){
                std::cout << dirpath << " No such file or directory" << std::endl;
            }
            return -1;
        }
        else
        {
            if(type != TYPE_FILE){
                disk.read(firstBlock,(uint8_t*)&cwd);
                cwdBlock = firstBlock;
                std::string nextPath = "";
                for (int i = 1; i < dirCounter ; i++){
                    nextPath += myArr[i];
                    if( i != dirCounter - 1){
                        nextPath += '/';
                    }
                }
                if(cd(nextPath, true) == -1){
                    if(!privateFunc){
                        std::cout << dirpath << " No such file or directory" << std::endl;
                    }
                }
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
    std::string fullPath = "";
    int childBlock = cwdBlock;
    while(childBlock != ROOT_BLOCK ){
        std::string temp = "";
        cd("..",true);
        for(int i = 0; i != BLOCK_SIZE / sizeof(dir_entry); i ++){
            if(cwd[i].first_blk == childBlock){
                temp += cwd[0].file_name;
                temp += '/';
                temp += fullPath;
                fullPath = temp;
                break;
            }
        }
    }
    
    std::cout << '/' + fullPath << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    int savedBlock = cwdBlock;
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    cwdBlock = savedBlock;
    disk.read(cwdBlock,(uint8_t*)&cwd);
    return 0;
}
