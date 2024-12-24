#include "src/myfs.h"

// 全局日志文件对象
std::ofstream log_file;
// 初始化日志文件并重定向 std::cout
void init_logging(const std::string& log_file_path) {
    log_file.open(log_file_path);
    if (log_file.is_open()) {
        std::cout.rdbuf(log_file.rdbuf()); // 重定向 cout
        std::cerr.rdbuf(log_file.rdbuf()); // 重定向 cerr
    } else {
        std::cerr << "Error: Unable to open log file: " << log_file_path << std::endl;
    }
}
int main() {
    init_logging("myfs.log");
    MyFileSystem fs("mydisk.img");

    // 格式化文件系统 (100MB, 10% inode)
    if (!fs.format(100 * 1024 * 1024, 10)) {
        std::cerr << "Failed to format file system." << std::endl;
        return 1;
    }

    // 挂载文件系统
    if (!fs.mount()) {
        std::cerr << "Failed to mount file system." << std::endl;
        return 1;
    }
    std::cout<<std::endl;
    std::string file1 = "/mydir/file1.txt";
    std::string file2 = "/mydir/subdir/file2.txt";
    std::string file3 = "/file3.txt";
    std::string file4 = "/mydir2/file4.txt";
    // 创建目录
    fs.mkdir("/mydir");
    fs.print_bitmap();
    fs.mkdir("/mydir/subdir");
    fs.print_bitmap();
    fs.mkdir("/mydir2");
    fs.print_bitmap();
    std::cout<<std::endl;
    // 创建文件
    fs.create(file1);
    fs.print_bitmap();
    fs.create(file2);
    fs.print_bitmap();
    fs.create(file3);
    fs.print_bitmap();
    fs.create(file4);
    std::cout<<std::endl;
    // 写入文件
    int fd1 = fs.open(file1);
    if (fd1 != -1) {
        fs.write(fd1, 0, 12, "Hello World!");
    }
    fs.print_bitmap();

    int fd2 = fs.open(file2);
    if (fd2 != -1) {
        fs.write(fd2, 0, 33, "This is a test file in subdir.");
    }
    fs.print_bitmap();
    
    int fd3 = fs.open(file3);
    if (fd3 != -1) {
        fs.write(fd3, 0, 33, "This is a test file in root.");
    }
    fs.print_bitmap();
    std::cout<<std::endl;
    // 列出目录内容
    fs.list("/");
    fs.list("/mydir");
    fs.list("/mydir2");
    fs.list("/mydir/subdir");
    std::cout<<std::endl;
    // 读取文件
    char buffer1[100] = {0};
    if (fd1 != -1) {
        fs.read(fd1, 6, 5, buffer1);  // 读取 "World"
        std::cout << "[Info] Read from file " <<file3<<" : "<< buffer1 << std::endl;
    }
    char buffer2[100] = {0};
    if (fd1 != -1) {
        fs.read(fd3, 0, 5, buffer2);  // 读取 "World"
        std::cout << "[Info] Read from file " <<file4<<" : "<< buffer2 << std::endl;
    }
    std::cout<<std::endl;
    // 删除文件
    fs.remove("/mydir/file1.txt");
    fs.remove("/mydir/subdir/file2.txt");
    fs.remove("/mydir2/file4.txt");
    // 删除目录
    fs.rmdir("/mydir/subdir");
    std::cout<<std::endl;
    // 再次列出目录内容
    fs.list("/mydir");
    fs.list("/mydir2");
    // 卸载文件系统
    fs.unmount();

    return 0;
}