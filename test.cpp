int
FS::cd(std::string dirpath,bool privateFunc  )
{
    if(!privateFunc){
        std::cout << "FS::cd(" << dirpath << ")\n";
    }
    if(dirpath == "." || dirpath == ".."){
        if(dirpath == "."){
            return 0;
        }
        else
        {
            std::string temp = "";
            temp += cwd[1].file_name[0];
            temp += cwd[1].file_name[1];
            if(temp != ".."){ // the root is the current working directory. 
            }
            else
            {
                disk.read(cwd[1].first_blk,(uint8_t*)&cwd);
                cwdBlock = cwd[0].first_blk;
            }
            return 0;
            
        }
        
    }
    if(dirpath == "/" ){
        disk.read(ROOT_BLOCK,(uint8_t*)&cwd);
        cwdBlock = ROOT_BLOCK;
        return 0;
    }
    std::string pathType = "";
    pathType += dirpath[0];
    pathType += dirpath[1];
    if(dirpath[0] == '/'){
        cd("/",true);

        std::string relativePath = "./";
        for(int i = 1; i != dirpath.length(); i++){
            relativePath += dirpath[i];
        }
        if (cd(relativePath, true) == -1){
            if(!privateFunc){
                std::cout << dirpath << ": No such file or directory" << std::endl;
            }
        }
        
    }
    else if(pathType == ".."){
        cd("..",true);
        std::string relativePath = "";
        relativePath.append(&dirpath[1]);
        if (cd(relativePath, true) == -1){
            if(!privateFunc){
                std::cout << dirpath << ": No such file or directory" << std::endl;
            }
        }
    }
    else if(pathType == "./"){
        
        std::string relativePath = "";
        int pathIndex = 2;
        char lastChar;
        while(lastChar != '/' && pathIndex != dirpath.length()){
            relativePath += dirpath[pathIndex];
            pathIndex ++;
            lastChar = dirpath[pathIndex];
        }
        if(lastChar == '/'){
            uint16_t firstBlock;
            uint32_t size;
            int type;
            int numberOfblocks;
            if(fileExists(firstBlock,size,numberOfblocks,relativePath,type)){
                std::string newPath = ".";
                for(int i = relativePath.length() + 2; i != dirpath.length(); i++){
                    newPath += dirpath[i];
                }
                if(type == TYPE_DIR){
                    disk.read(firstBlock,(uint8_t*)&cwd);
                    cwdBlock = firstBlock;
                    if(cd(newPath,true) == -1){
                        if(!privateFunc){
                            std::cout << dirpath << ": No such file or directory" << std::endl;
                        }
                    }
                }                
            }
        }
        else
        {
            // std::cout << "breakepoint 8" << std::endl;
            uint16_t firstBlock;
            uint32_t size;
            int type;
            int numberOfblocks;
            if(fileExists(firstBlock,size,numberOfblocks,relativePath,type) && type == TYPE_DIR){
                disk.read(firstBlock,(uint8_t*)&cwd);
                cwdBlock = firstBlock;
            }
            else{
                if(!privateFunc){
                    std::cout << dirpath << ": No such file or directory" << std::endl;
                }
                return -1;
            }
        }
        
    }
    else{
        std::string newPath = "./";
        newPath+= dirpath;
        if (cd(newPath,true) == -1){
            if(!privateFunc){
                std::cout << dirpath << ": No such file or directory" << std::endl;
            }
        }
    }
    
    return 0;
}