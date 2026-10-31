#import <Foundation/Foundation.h>
#import <string>
#import <vector>

using namespace std;

class 环境隐藏类型 {
public:
    bool 初始化();
    
    bool 检测roothide环境();
    bool 检测巨魔环境();
    bool 检测kfd环境();
    
    bool 隐藏roothide特征();
    bool 隐藏巨魔特征();
    bool 隐藏kfd特征();
    
    bool 隐藏所有越狱特征();
    
    void 打印检测结果();
    
private:
    bool kfd已初始化 = false;
    
    vector<string> roothide文件路径;
    vector<string> roothide环境变量;
    bool roothide已检测 = false;
    
    vector<string> 巨魔文件路径;
    vector<string> 巨魔应用标识;
    bool 巨魔已检测 = false;
    
    bool kfd已检测 = false;
    
    static vector<string> 已隐藏应用列表;
    static vector<string> 已隐藏动态库列表;
    
    bool 隐藏文件路径(const string& 路径);
    bool 隐藏环境变量(const string& 变量名);
    bool 隐藏应用标识(const string& 标识);
    
    bool 检测文件是否存在(const string& 路径);
    bool 检测环境变量是否存在(const string& 变量名);
    bool 检测应用是否安装(const string& 标识);
};

extern 环境隐藏类型 环境隐藏;
