#ifndef MYFS_H
#define MYFS_H
#include <iostream>
#include <fstream>
#include <cstring>
#include <ctime>
#include <cmath>
const int BLOCK_SIZE = 4096;  // 数据块大小
const int MAX_FILE_NAME_LENGTH = 255;

// 魔数，用于标识文件系统
const unsigned int MAGIC_NUMBER = 0xDEADBEEF;

// 文件类型
enum FileType {
    DIRECTORY,
    REGULAR_FILE
};

// 超级块
struct Superblock {
    unsigned int magic_number;
    unsigned int total_size;
    unsigned int block_size;
    unsigned int inode_count;
    unsigned int data_block_count;
    unsigned int free_inode_start;
    unsigned int free_data_block_start;
    unsigned int free_inode_count;
    unsigned int free_data_block_count;

    Superblock() : magic_number(MAGIC_NUMBER), total_size(0), block_size(BLOCK_SIZE), inode_count(0),
                     data_block_count(0), free_inode_start(0), free_data_block_start(0), free_inode_count(0),
                     free_data_block_count(0) {}
};

// 目录项
struct DirectoryEntry {
    char filename[MAX_FILE_NAME_LENGTH + 1];
    unsigned int inode_number;

    DirectoryEntry() : inode_number(0) {
        memset(filename, 0, sizeof(filename));
    }
};

// 索引节点
struct Inode {
    FileType type;
    unsigned int size;
    unsigned int permissions;
    time_t created_time;
    time_t modified_time;
    time_t accessed_time;
    unsigned int direct_blocks[10]; // 直接块指针
    unsigned int indirect_block;   // 一级间接块指针 (可以扩展为多级)
    char path[255]; //文件路径
    bool used;

    Inode() : type(REGULAR_FILE), size(0), permissions(0644), created_time(0), modified_time(0),
                accessed_time(0), indirect_block(0),path(""),used(false) {
        for (int i = 0; i < 10; i++) {
            direct_blocks[i] = 0;
        }
    }
};
// 定义常量

const int INODE_SIZE = sizeof(Inode);
const int SUPERBLOCK_SIZE = sizeof(Superblock);
const int DIRECTORY_ENTRY_SIZE = sizeof(DirectoryEntry);
class MyFileSystem {
private:
    std::fstream disk;      // 磁盘文件
    std::string disk_file_path; // 磁盘文件路径
    Superblock superblock;  // 超级块

public:
    MyFileSystem(const std::string& disk_path) : disk_file_path(disk_path) {}

    // 初始化文件系统
    bool format(unsigned int disk_size, unsigned int inode_percentage);

    // 加载文件系统
    bool mount();

    // 卸载文件系统
    bool unmount();

    // 创建目录
    bool mkdir(const std::string& path);

    // 删除目录
    bool rmdir(const std::string& path);

    // 创建文件
    bool create(const std::string& path);

    // 删除文件
    bool remove(const std::string& path);

    // 打开文件 (简化版，仅返回 inode 编号)
    int open(const std::string& path);

    // 读取文件
    bool read(int inode_number, unsigned int offset, unsigned int length, char* buffer);
    bool read(int inode_number, char* buffer);
    // 写入文件
    bool write(int inode_number, unsigned int offset, unsigned int length, const char* buffer);

    // 列出目录内容
    bool list(const std::string& path);

    //输出位图
    void print_bitmap();

private:
    // 从磁盘读取超级块
    void read_superblock();

    // 将超级块写入磁盘
    void write_superblock();

    // 读取 inode
    Inode read_inode(unsigned int inode_number);

    // 写入 inode
    void write_inode(unsigned int inode_number, const Inode& inode);

    // 读取数据块
    void read_data_block(unsigned int block_number, char* buffer);

    // 写入数据块
    void write_data_block(unsigned int block_number, const char* buffer);

    // 分配一个 inode
    unsigned int allocate_inode(FileType type);

    // 释放一个 inode
    void free_inode(unsigned int inode_number);

    // 分配一个数据块
    unsigned int allocate_data_block();

    // 释放一个数据块
    void free_data_block(unsigned int block_number);

    // 根据路径查找 inode 编号
    int path_to_inode(const std::string& path);

    // 获取父目录的 inode 编号
    int get_parent_inode(const std::string& path);

    // 更新位图
    void update_bitmap(unsigned int block_number, bool allocated);

    // 检查位图
    bool check_bitmap(unsigned int block_number);
    //计算位图区大小
    unsigned int calculate_bitmap_size() {
        return (superblock.data_block_count + 7) / 8; // 向上取整
    }
};

#endif // MYFS_H