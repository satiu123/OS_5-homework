#include "src/myfs.h"
#include <vector>

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
int main(){
    std::string request;
    std::string current_path = "/";
    MyFileSystem fs("mydisk.img");
    //若文件系统不存在则格式化
    if (std::filesystem::exists("mydisk.img")){
        if (!fs.mount()) {
            std::cerr << "Failed to mount file system." << std::endl;
            return 1;
        }
    }else{
        // 格式：100MB 磁盘，inode 占比 10%
        std::cout <<"File system does not exist. Formatting..." << std::endl;
        if (!fs.format(100 * 1024 * 1024, 10)) {
            std::cerr << "Failed to format file system." << std::endl;
            return 1;
        }
        if (!fs.mount()) {
            std::cerr << "Failed to mount file system." << std::endl;
            return 1;
        }
    }
    while(true){
        std::cout<<"PS "<<current_path<<"> ";
        std::getline(std::cin, request);
        //大量if else if 绝赞发行中
        //后面再改
        std::vector<std::string> request_split = split(request, " ");
        if (request_split.size() == 1){
            if(request == "exit"){
                break;
            }else if (request == "print_bitmap"){
                fs.print_bitmap();
                continue;
            }else if (request =="ls") {
                fs.list(current_path);
                continue;
            }else {
                std::cerr<<"Invalid Command."<<std::endl;
            }
            continue;
        }else if(request_split.size()>=2){
            if (request_split[1][0] != '/' and request_split[0] != "cd"){
                request_split[1] = current_path + request_split[1];
            }
            if(request_split[0] == "mkdir"){
                fs.mkdir(request_split[1]);
            }else if(request_split[0] == "rmdir"){
                fs.rmdir(request_split[1]);
            }else if(request_split[0] == "create"){
                fs.create(request_split[1]);
                if (request_split.size() >= 3){
                    int fd = fs.open(request_split[1]);
                    std::string buffer;
                    for(int i = 2; i < request_split.size(); i++){
                        buffer += request_split[i];
                        if (i != request_split.size() - 1)
                            buffer += " ";
                    }
                    if (fd != -1) {
                        fs.write(fd, 0, buffer.size(), buffer.c_str());
                    }
                }
            }else if(request_split[0] == "remove"){
                fs.remove(request_split[1]);
            }else if(request_split[0] == "open"){
                fs.open(request_split[1]);
            }else if (request_split[0] == "ls"){
                    fs.list(request_split[1]);
            }else if(request_split[0] == "read"){
                int fd = fs.open(request_split[1]);
                if (fd != -1) {
                    char buffer[100] = {0};
                    fs.read(fd, 0, 100, buffer);
                    std::cout << "[Info] Read from file " <<request_split[1]<<" : "<< buffer << std::endl;
                }
            }else if(request_split[0] == "write"){
                int fd = fs.open(request_split[1]);
                if (fd != -1) {
                    std::string buffer;
                    for(int i = 3; i < request_split.size(); i++){
                        buffer += request_split[i];
                        if (i != request_split.size() - 1)
                            buffer += " ";
                    }
                    fs.write(fd, 0, buffer.size(), buffer.c_str());
                }
            }else if(request_split[0] == "cd"){
                fs.change_dir(current_path,request_split[1]);
            }else {
                std::cerr<<"Invalid Command."<<std::endl;
            }
        }
    }
    fs.mount();
    return 0;
}