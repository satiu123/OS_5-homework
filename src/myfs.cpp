#include "myfs.h"
#include <iomanip>
#include <vector>
// 初始化文件系统
bool MyFileSystem::format(unsigned int disk_size, unsigned int inode_percentage) {
    if (disk.is_open()) {
        disk.close();
    }

    disk.open(disk_file_path, std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
    if (!disk.is_open()) {
        std::cerr << "Unable to create disk file." << std::endl;
        return false;
    }

    superblock.total_size = disk_size;
    superblock.inode_count = (disk_size * inode_percentage) / (100 * INODE_SIZE);

    // 计算位图大小和块数
    unsigned int bitmap_size = calculate_bitmap_size();
    unsigned int bitmap_blocks = (bitmap_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // 预留位图空间
    superblock.free_inode_start = SUPERBLOCK_SIZE + bitmap_blocks * BLOCK_SIZE;
    superblock.data_block_count = (disk_size - superblock.free_inode_start - superblock.inode_count * INODE_SIZE) / BLOCK_SIZE;
    superblock.free_inode_count = superblock.inode_count;
    superblock.free_data_block_count = superblock.data_block_count;
    superblock.free_data_block_start = superblock.free_inode_start + superblock.inode_count * INODE_SIZE;

    write_superblock();

    // 初始化位图 (所有块标记为空闲)
    char empty_block[BLOCK_SIZE] = {0};
    for (unsigned int i = 0; i < bitmap_blocks; i++) {
        write_data_block(i - superblock.inode_count * INODE_SIZE / BLOCK_SIZE, empty_block);
    }
    // 初始化 inode 
    Inode empty_inode;
    for (unsigned int i = 1; i < superblock.inode_count; i++) {
        write_inode(i, empty_inode);
    }
    // 初始化根目录
    Inode root_inode;
    root_inode.type = DIRECTORY;
    root_inode.size = 0;
    root_inode.created_time = time(nullptr);
    root_inode.modified_time = time(nullptr);
    root_inode.accessed_time = time(nullptr);
    write_inode(0, root_inode);
    superblock.free_inode_count--;

    for (unsigned int i = 0; i < superblock.data_block_count; i++) {
        write_data_block(i + superblock.free_data_block_start / BLOCK_SIZE, empty_block);
    }

    write_superblock();

    std::cout << "File system formatted successfully." << std::endl;
    std::cout << "Total size: " << superblock.total_size << " bytes" << std::endl;
    std::cout << "Inode count: " << superblock.inode_count << std::endl;
    std::cout << "Data block count: " << superblock.data_block_count << std::endl;

    return true;
}
// 加载文件系统
bool MyFileSystem::mount() {
    if (disk.is_open()) {
        disk.close();
    }
    disk.open(disk_file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) {
        std::cerr << "Unable to open disk file." << std::endl;
        return false;
    }

    // 读取超级块
    read_superblock();

    // 验证魔数
    if (superblock.magic_number != MAGIC_NUMBER) {
        std::cerr << "Invalid file system format." << std::endl;
        disk.close();
        return false;
    }

    std::cout << "File system mounted successfully." << std::endl;
    return true;
}

// 卸载文件系统
bool MyFileSystem::unmount() {
    if (disk.is_open()) {
        disk.close();
        std::cout << "File system unmounted successfully." << std::endl;
    }
    return true;
}

// 从磁盘读取超级块
void MyFileSystem::read_superblock() {
    disk.seekg(0, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&superblock), sizeof(Superblock));
}

// 将超级块写入磁盘
void MyFileSystem::write_superblock() {
    disk.seekp(0, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&superblock), sizeof(Superblock));
    disk.flush();
}

// 读取 inode
Inode MyFileSystem::read_inode(unsigned int inode_number) {
    Inode inode;
    disk.seekg(superblock.free_inode_start + inode_number * sizeof(Inode), std::ios::beg);
    disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
    return inode;
}

// 写入 inode
void MyFileSystem::write_inode(unsigned int inode_number, const Inode& inode) {
    disk.seekp(superblock.free_inode_start + inode_number * sizeof(Inode), std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    disk.flush();
}

// 读取数据块
void MyFileSystem::read_data_block(unsigned int block_number, char* buffer) {
    disk.seekg(superblock.free_data_block_start + block_number * BLOCK_SIZE, std::ios::beg);
    disk.read(buffer, BLOCK_SIZE);
}

// 写入数据块
void MyFileSystem::write_data_block(unsigned int block_number, const char* buffer) {
    disk.seekp(superblock.free_data_block_start + block_number * BLOCK_SIZE, std::ios::beg);
    disk.write(buffer, BLOCK_SIZE);
    disk.flush();
}
// 分配一个 inode
unsigned int MyFileSystem::allocate_inode(FileType type) {
    if (superblock.free_inode_count == 0) {
        std::cerr << "No free inode available." << std::endl;
        return -1;
    }

    for (unsigned int i = 1; i < superblock.inode_count; i++) {
        Inode inode = read_inode(i);
        if (inode.type == REGULAR_FILE and inode.size == 0 and !inode.used) { // 未使用的 inode
            superblock.free_inode_count--;
            inode.type = type;
            write_inode(i, inode);
            write_superblock();
            return i;
        }
    }
    std::cerr<<"Unable to allocate inode."<<std::endl;
    return -1;
}
// 释放一个 inode
void MyFileSystem::free_inode(unsigned int inode_number) {
    Inode inode = read_inode(inode_number);
    
    // 释放数据块
    for (int i = 0; i < 10; i++) {
        if (inode.direct_blocks[i] != 0) {
            free_data_block(inode.direct_blocks[i]);
            inode.direct_blocks[i] = 0;
        }
    }
    if (inode.indirect_block != 0) {
        // todo 处理间接块... 
    }

    // 将 inode 标记为空闲
    inode.type = REGULAR_FILE;
    inode.size = 0;
    inode.permissions = 0;
    inode.used=false;
    write_inode(inode_number, inode);

    superblock.free_inode_count++;
    write_superblock();
}
// 分配一个数据块
unsigned int MyFileSystem::allocate_data_block() {
    if (superblock.free_data_block_count == 0) {
        std::cerr << "No free data blocks available." << std::endl;
        return -1;
    }

    unsigned int bitmap_size = calculate_bitmap_size();
    char bitmap_block[BLOCK_SIZE];

    for (unsigned int i = 0; i < superblock.data_block_count; i++) {
        // 逐块读取位图
        if (i % (BLOCK_SIZE * 8) == 0) {
            read_data_block(i / (BLOCK_SIZE * 8) + 1 + superblock.inode_count * INODE_SIZE / BLOCK_SIZE, bitmap_block);
        }

        unsigned int bit_index = i % (BLOCK_SIZE * 8);
        unsigned int byte_index = bit_index / 8;
        unsigned int bit_offset = bit_index % 8;

        if (!(bitmap_block[byte_index] & (1 << bit_offset))) { // 找到一个空闲的数据块
            update_bitmap(i, true); // 更新位图
            superblock.free_data_block_count--;
            write_superblock();
            return i;
        }
    }

    std::cerr << "Unable to allocate data block." << std::endl;
    return -1;
}

// 释放一个数据块
void MyFileSystem::free_data_block(unsigned int block_number) {
    update_bitmap(block_number, false);
    superblock.free_data_block_count++;
    write_superblock();
}
// 更新位图
void MyFileSystem::update_bitmap(unsigned int block_number, bool allocated) {
    unsigned int bitmap_block_number = block_number / (BLOCK_SIZE * 8) + 1 + superblock.inode_count * INODE_SIZE/BLOCK_SIZE;
    unsigned int bit_index = block_number % (BLOCK_SIZE * 8);
    unsigned int byte_index = bit_index / 8;
    unsigned int bit_offset = bit_index % 8;

    char block[BLOCK_SIZE];
    read_data_block(bitmap_block_number, block);

    if (allocated) {
        block[byte_index] |= (1 << bit_offset);
    } else {
        block[byte_index] &= ~(1 << bit_offset);
    }

    write_data_block(bitmap_block_number, block);
}
void MyFileSystem::print_bitmap(){
    std::cout << "Bitmap status:" << std::endl;
    for (unsigned int i = 0; i < 10; i++) {
        std::cout << check_bitmap(i);
        if ((i + 1) % (BLOCK_SIZE * 8) == 0) {
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}
bool MyFileSystem::check_bitmap(unsigned int block_number){
        unsigned int bitmap_block_number = block_number / (BLOCK_SIZE * 8) + 1 + superblock.inode_count * INODE_SIZE / BLOCK_SIZE;
        unsigned int bit_index = block_number % (BLOCK_SIZE * 8);
        unsigned int byte_index = bit_index / 8;
        unsigned int bit_offset = bit_index % 8;

        char block[BLOCK_SIZE];
        read_data_block(bitmap_block_number, block);

        return (block[byte_index] & (1 << bit_offset)) != 0;
    }
// 根据路径查找 inode 编号
int MyFileSystem::path_to_inode(const std::string& path) {
    if (path == "/") {
        return 0; // 根目录的 inode 编号为 0
    }

    std::string current_path = "/";
    int current_inode_number = 0;
    std::string delimiter = "/";
    size_t pos = 0;
    std::string token;
    // std::string path_copy{path};
    size_t start = 1;
    size_t end = path.find(delimiter, start);
    // 分割路径
    while (end != std::string::npos) {
        token = path.substr(start, end - start);

        // 查找当前目录下的目录项
        Inode current_inode = read_inode(current_inode_number);
        if (current_inode.type != DIRECTORY) {
            std::cerr << "" << current_path << " is not a directory." << std::endl;
            return -1;
        }
        bool found = false;
        for (int i = 0; i < 10; i++) {
            if(current_inode.direct_blocks[i] == 0) continue;
            char block_buffer[BLOCK_SIZE];
            read_data_block(current_inode.direct_blocks[i], block_buffer);
            for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
                DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
                if (entry->inode_number != 0 && entry->filename == token) {
                    current_inode_number = entry->inode_number;
                    current_path += token + "/";
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        // 没找到对应的目录项
        if (!found) {
            std::cerr << "" << token << " not found in " << current_path << std::endl;
            return -1;
        }
        start = end + 1;
        end = path.find(delimiter, start);
    }

    // 查找最后一个目录项
    token = path.substr(start);
    if (!token.empty()) {
        Inode current_inode = read_inode(current_inode_number);
        if (current_inode.type != DIRECTORY) {
            std::cerr << "" << current_path << " is not a directory." << std::endl;
            return -1;
        }
        bool found = false;
        for (int i = 0; i < 10; i++) {
            if(current_inode.direct_blocks[i] == 0) continue;
            char block_buffer[BLOCK_SIZE];
            read_data_block(current_inode.direct_blocks[i], block_buffer);
            for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
                DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
                if (entry->inode_number != 0 && entry->filename == token) {
                    current_inode_number = entry->inode_number;
                    found = true;
                    break;
                }
            }
             if (found) break;
        }
        if (!found) {
            std::cerr << "" << token << " not found in " << current_path << std::endl;
            return -1;
        }
    }

    return current_inode_number;
}

// 获取父目录的 inode 编号
int MyFileSystem::get_parent_inode(const std::string& path) {
    size_t last_slash_pos = path.find_last_of('/');
    if (last_slash_pos == std::string::npos) {
        return -1; // 无效路径
    }
    if (last_slash_pos == 0) {
        return 0; // 父目录是根目录
    }
    std::string parent_path = path.substr(0, last_slash_pos);
    return path_to_inode(parent_path);
}

// 创建目录
bool MyFileSystem::mkdir(const std::string& path) {
    // 检查目录是否已存在
    if (path_to_inode(path) != -1) {
        std::cerr << "Directory already exists." << std::endl;
        return false;
    }

    // 获取父目录的 inode 编号
    int parent_inode_number = get_parent_inode(path);
    if (parent_inode_number == -1) {
        std::cerr << "Invalid path." << std::endl;
        return false;
    }

    // 分配一个新的 inode
    int new_inode_number = allocate_inode(DIRECTORY);
    if (new_inode_number == -1) {
        return false;
    }
    // 在父目录中添加新的目录项
    Inode parent_inode = read_inode(parent_inode_number);

    // 如果是根目录下的第一个目录项，需要为根目录分配数据块
    if (parent_inode_number == 0 && parent_inode.size == 0) {
        unsigned int new_block = allocate_data_block();
        if (new_block == -1) {
            free_inode(new_inode_number);
            return false;
        }
        parent_inode.direct_blocks[0] = new_block;
    }

    bool entry_added = false;
    for (int i = 0; i < 10; i++) {
        if (parent_inode.direct_blocks[i] == 0) {
             // 分配一个新的数据块给父目录
            unsigned int new_block = allocate_data_block();
            if (new_block == -1) {
                free_inode(new_inode_number);
                return false;
            }
            parent_inode.direct_blocks[i] = new_block;
        }
        char block_buffer[BLOCK_SIZE];
        read_data_block(parent_inode.direct_blocks[i], block_buffer);
       
        for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
            DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
            if (entry->inode_number == 0) {
                std::string filename = path.substr(path.find_last_of('/') + 1);
                if (filename.length() > MAX_FILE_NAME_LENGTH) {
                    std::cerr << "Filename too long." << std::endl;
                    free_inode(new_inode_number);
                    return false;
                }
                strcpy(entry->filename, filename.c_str());
                entry->inode_number = new_inode_number;
                write_data_block(parent_inode.direct_blocks[i], block_buffer);
                parent_inode.size += DIRECTORY_ENTRY_SIZE;
                parent_inode.modified_time = time(nullptr);
                write_inode(parent_inode_number, parent_inode);
                entry_added = true;
                break;
            }
        }
        if (entry_added) break;
    }

    if (!entry_added) {
        std::cerr << "Parent directory is full." << std::endl;
        free_inode(new_inode_number);
        return false;
    }

    // 初始化新目录的 inode
    Inode new_inode = read_inode(new_inode_number);
    new_inode.type = DIRECTORY;
    new_inode.size = 0; // 初始大小为 0
    new_inode.modified_time = time(nullptr);
    new_inode.used=true;
    strcpy(new_inode.path,path.c_str());
    write_inode(new_inode_number, new_inode);

    std::cout << "Directory created: " << path <<" Inode Number: "<<new_inode_number<<std::endl;
    return true;
}

// 删除目录
bool MyFileSystem::rmdir(const std::string& path) {
    // 检查目录是否存在
    int inode_number = path_to_inode(path);
    if (inode_number == -1) {
        std::cerr << "Directory does not exist." << std::endl;
        return false;
    }

    // 检查是否为目录
    Inode inode = read_inode(inode_number);
    if (inode.type != DIRECTORY) {
        std::cerr << "Not a directory." << std::endl;
        return false;
    }

    // 检查目录是否为空
    if (inode.size > 0) {
        // 遍历目录项，检查是否有文件或子目录
        for(int i=0; i < 10; i++){
            if(inode.direct_blocks[i] == 0) continue;
            char block_buffer[BLOCK_SIZE];
            read_data_block(inode.direct_blocks[i], block_buffer);
            for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
                DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
                if (entry->inode_number != 0) {
                    std::cerr << "Directory is not empty." << std::endl;
                    return false;
                }
            }
        }
    }

    // 获取父目录的 inode 编号
    int parent_inode_number = get_parent_inode(path);
    if (parent_inode_number == -1) {
        std::cerr << "Invalid path." << std::endl;
        return false;
    }

    // 从父目录中删除目录项
    Inode parent_inode = read_inode(parent_inode_number);
    bool entry_removed = false;
    for(int i = 0; i < 10; i++){
        if(parent_inode.direct_blocks[i] == 0) continue;
        char block_buffer[BLOCK_SIZE];
        read_data_block(parent_inode.direct_blocks[i], block_buffer);
        for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
            DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
            if (entry->inode_number == inode_number) {
                entry->inode_number = 0; // 将 inode 编号设置为 0 表示该目录项为空闲
                memset(entry->filename, 0, sizeof(entry->filename));
                write_data_block(parent_inode.direct_blocks[i], block_buffer);
                parent_inode.size -= DIRECTORY_ENTRY_SIZE;
                parent_inode.modified_time = time(nullptr);
                write_inode(parent_inode_number, parent_inode);
                entry_removed = true;
                break;
            }
        }
        if (entry_removed) break;
    }

    if (!entry_removed) {
        std::cerr << "Failed to remove directory entry from parent." << std::endl;
        return false;
    }

    // 释放 inode
    free_inode(inode_number);

    std::cout << "Directory removed: " << path << std::endl;
    return true;
}
//改变目录
bool MyFileSystem::change_dir(std::string&cur,std::string& des){
    if (des==".."){
        if (cur=="/") return false;
        cur=cur.substr(0,cur.size()-1);
        cur=cur.substr(0,cur.find_last_of('/'));
        if (cur=="") cur="/";
        return true;
    }
    if (des[0]!='/') des=cur+des;
    int inode_number = path_to_inode(des);
    if (inode_number == -1) {
        std::cerr << "Directory does not exist." << std::endl;
        return false;
    }

    Inode inode = read_inode(inode_number);
    if (inode.type != DIRECTORY) {
        std::cerr << "Not a directory." << std::endl;
        return false;
    }
    cur=des+"/";
    return true;
}

// 创建文件
bool MyFileSystem::create(const std::string& path) {
    // 检查文件是否已存在
    if (path_to_inode(path) != -1) {
        std::cerr << "File already exists." << std::endl;
        return false;
    }

    // 获取父目录的 inode 编号
    int parent_inode_number = get_parent_inode(path);
    if (parent_inode_number == -1) {
        std::cerr << "Invalid path." << std::endl;
        return false;
    }

    // 分配一个新的 inode
    int new_inode_number = allocate_inode(REGULAR_FILE);
    if (new_inode_number == -1) {
        return false;
    }

    // 在父目录中添加新的目录项
    Inode parent_inode = read_inode(parent_inode_number);

    // 如果是根目录下的第一个文件项，需要为根目录分配数据块
    if (parent_inode_number == 0 && parent_inode.size == 0) {
        unsigned int new_block = allocate_data_block();
        if (new_block == -1) {
            free_inode(new_inode_number);
            return false;
        }
        parent_inode.direct_blocks[0] = new_block;
    }
    
    bool entry_added = false;
    for (int i = 0; i < 10; i++) {
        if (parent_inode.direct_blocks[i] == 0) {
             // 分配一个新的数据块给父目录
            unsigned int new_block = allocate_data_block();
            if (new_block == -1) {
                free_inode(new_inode_number);
                return false;
            }
            parent_inode.direct_blocks[i] = new_block;
        }
        char block_buffer[BLOCK_SIZE];
        read_data_block(parent_inode.direct_blocks[i], block_buffer);
        for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
            DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
            if (entry->inode_number == 0) {
                std::string filename = path.substr(path.find_last_of('/') + 1);
                if (filename.length() > MAX_FILE_NAME_LENGTH) {
                    std::cerr << "Filename too long." << std::endl;
                    free_inode(new_inode_number);
                    return false;
                }
                strcpy(entry->filename, filename.c_str());
                entry->inode_number = new_inode_number;
                write_data_block(parent_inode.direct_blocks[i], block_buffer);
                parent_inode.size += DIRECTORY_ENTRY_SIZE;
                parent_inode.modified_time = time(nullptr);
                write_inode(parent_inode_number, parent_inode);
                entry_added = true;
                break;
            }
        }
        if (entry_added) break;
    }

    if (!entry_added) {
        std::cerr << "Parent directory is full." << std::endl;
        free_inode(new_inode_number);
        return false;
    }

    // 初始化新文件的 inode
    Inode new_inode = read_inode(new_inode_number);
    new_inode.type = REGULAR_FILE;
    new_inode.size = 0; // 初始大小为 0
    new_inode.modified_time = time(nullptr);
    new_inode.used=true;
    strcpy(new_inode.path,path.c_str());
    write_inode(new_inode_number, new_inode);

    std::cout << "File created: " << path<< " Inode Number: "<<new_inode_number << std::endl;
    return true;
}

// 删除文件
bool MyFileSystem::remove(const std::string& path) {
    // 检查文件是否存在
    int inode_number = path_to_inode(path);
    if (inode_number == -1) {
        std::cerr << "File does not exist." << std::endl;
        return false;
    }

    // 检查是否为普通文件
    Inode inode = read_inode(inode_number);
    if (inode.type != REGULAR_FILE) {
        std::cerr << "Not a regular file." << std::endl;
        return false;
    }

    // 获取父目录的 inode 编号
    int parent_inode_number = get_parent_inode(path);
    if (parent_inode_number == -1) {
        std::cerr << "Invalid path." << std::endl;
        return false;
    }

    // 从父目录中删除目录项
    Inode parent_inode = read_inode(parent_inode_number);
    bool entry_removed = false;
    for(int i = 0; i < 10; i++){
        if(parent_inode.direct_blocks[i] == 0) continue;
        char block_buffer[BLOCK_SIZE];
        read_data_block(parent_inode.direct_blocks[i], block_buffer);
        for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
            DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
            if (entry->inode_number == inode_number) {
                entry->inode_number = 0; // 将 inode 编号设置为 0 表示该目录项为空闲
                memset(entry->filename, 0, sizeof(entry->filename));
                write_data_block(parent_inode.direct_blocks[i], block_buffer);
                parent_inode.size -= DIRECTORY_ENTRY_SIZE;
                parent_inode.modified_time = time(nullptr);
                write_inode(parent_inode_number, parent_inode);
                entry_removed = true;
                break;
            }
        }
        if(entry_removed) break;
    }

    if (!entry_removed) {
        std::cerr << "Failed to remove directory entry from parent." << std::endl;
        return false;
    }

    // 释放 inode (包括释放数据块)
    free_inode(inode_number);

    std::cout << "File removed: " << path << std::endl;
    return true;
}

// 打开文件 (简化版，仅返回 inode 编号)
int MyFileSystem::open(const std::string& path) {
    int inode_number = path_to_inode(path);
    if (inode_number == -1) {
        std::cerr << "File does not exist." << std::endl;
        return -1;
    }

    Inode inode = read_inode(inode_number);
    if (inode.type != REGULAR_FILE) {
        std::cerr << "Not a regular file." << std::endl;
        return -1;
    }
    
    //更新访问时间
    inode.accessed_time = time(nullptr);
    write_inode(inode_number, inode);

    return inode_number;
}

// 读取文件
bool MyFileSystem::read(int inode_number, unsigned int offset, unsigned int length, char* buffer) {
    Inode inode = read_inode(inode_number);

    // 检查偏移量是否越界
    if (offset >= inode.size) {
        std::cerr << "Offset out of range." << std::endl;
        return false;
    }

    // 计算实际读取的长度
    unsigned int bytes_to_read = length;
    if (offset + length > inode.size) {
        bytes_to_read = inode.size - offset;
    }

    unsigned int start_block = offset / BLOCK_SIZE;
    unsigned int end_block = (offset + bytes_to_read - 1) / BLOCK_SIZE;
    unsigned int block_offset = offset % BLOCK_SIZE;
    unsigned int buffer_offset = 0;

    for (unsigned int i = start_block; i <= end_block; i++) {
        unsigned int block_number;
        if (i < 10) {
            block_number = inode.direct_blocks[i];
        } else {
            // todo 需要读取一级间接块
            
        }

        if (block_number == 0) {
            std::cerr << "Data block not allocated." << std::endl;
            return false;
        }

        char block_buffer[BLOCK_SIZE];
        read_data_block(block_number, block_buffer);

        unsigned int bytes_in_block = BLOCK_SIZE - block_offset;
        if (bytes_in_block > bytes_to_read - buffer_offset) {
            bytes_in_block = bytes_to_read - buffer_offset;
        }

        memcpy(buffer + buffer_offset, block_buffer + block_offset, bytes_in_block);
        buffer_offset += bytes_in_block;
        block_offset = 0; // 后续的块都是从头开始读取
    }
    
    //更新访问时间
    inode.accessed_time = time(nullptr);
    write_inode(inode_number, inode);
    
    return true;
}

// 写入文件
bool MyFileSystem::write(int inode_number, unsigned int offset, unsigned int length, const char* buffer) {
    Inode inode = read_inode(inode_number);

    unsigned int end_offset = offset + length;
    unsigned int start_block = offset / BLOCK_SIZE;
    unsigned int end_block = (end_offset - 1) / BLOCK_SIZE;
    unsigned int block_offset = offset % BLOCK_SIZE;
    unsigned int buffer_offset = 0;

    for (unsigned int i = start_block; i <= end_block; i++) {
        unsigned int block_number;
        if (i < 10) {
            if (inode.direct_blocks[i] == 0) {
                // 分配一个新的数据块
                unsigned int new_block = allocate_data_block();
                if (new_block == -1) {
                    return false;
                }
                inode.direct_blocks[i] = new_block;
            }
            block_number = inode.direct_blocks[i];
        } else {
            // todo 需要使用一级间接块
            
        }
        
        char block_buffer[BLOCK_SIZE];
        // 如果不是从块的起始位置开始写入，需要先读取原来的数据
        if (block_offset > 0 || (block_offset + (length-buffer_offset) < BLOCK_SIZE)) {
            read_data_block(block_number, block_buffer);
        }

        unsigned int bytes_in_block = BLOCK_SIZE - block_offset;
        if (bytes_in_block > length - buffer_offset) {
            bytes_in_block = length - buffer_offset;
        }

        memcpy(block_buffer + block_offset, buffer + buffer_offset, bytes_in_block);
        write_data_block(block_number, block_buffer);

        buffer_offset += bytes_in_block;
        block_offset = 0; // 后续的块都是从头开始写入
    }

    // 更新 inode 的大小和修改时间
    if (end_offset > inode.size) {
        inode.size = end_offset;
    }
    inode.modified_time = time(nullptr);
    write_inode(inode_number, inode);

    return inode_number;
}

// 列出目录内容
bool MyFileSystem::list(const std::string& path) {
    int inode_number = path_to_inode(path);
    if (inode_number == -1) {
        std::cerr << "Directory does not exist." << std::endl;
        return false;
    }

    Inode inode = read_inode(inode_number);
    if (inode.type != DIRECTORY) {
        std::cerr << "Not a directory." << std::endl;
        return false;
    }

    inode.accessed_time = time(nullptr);
    write_inode(inode_number, inode);

    std::cout << "Listing directory: " << path << std::endl;
    // 存储目录项信息的 vector
    std::vector<std::vector<std::string>> rows;

    // 获取表头
    rows.push_back({"Type", "Permissions", "Size", "Created Time", "Name"});

    // 遍历目录项并收集信息
    for (int i = 0; i < 10; i++) {
        if (inode.direct_blocks[i] == 0) continue;
        char block_buffer[BLOCK_SIZE];
        read_data_block(inode.direct_blocks[i], block_buffer);

        for (int j = 0; j < BLOCK_SIZE / DIRECTORY_ENTRY_SIZE; j++) {
            DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(block_buffer + j * DIRECTORY_ENTRY_SIZE);
            if (entry->inode_number != 0) {
                Inode entry_inode = read_inode(entry->inode_number);
                rows.push_back({
                    (entry_inode.type == DIRECTORY ? "d" : "-"),
                    std::to_string(entry_inode.permissions),
                    std::to_string(entry_inode.size),
                    std::asctime(std::localtime(&entry_inode.created_time)),
                    entry->filename
                });
            }
        }
    }

    // 计算每列的最大宽度
    std::vector<size_t> widths(rows[0].size(), 0);
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }

    // 打印表格
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
             // 移除多余的换行符
            std::string cell = row[i];
            cell.erase(std::remove(cell.begin(), cell.end(), '\n'), cell.end());

            std::cout << std::left << std::setw(widths[i] + 2) << cell;
        }
        std::cout << std::endl;
    }

    return true;
}