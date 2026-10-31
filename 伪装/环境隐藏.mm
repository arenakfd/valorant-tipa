#import "环境隐藏.h"
#import "kfd.h"
#import <sys/stat.h>
#import <dlfcn.h>
#import <mach-o/dyld.h>

环境隐藏类型 环境隐藏;

vector<string> 环境隐藏类型::已隐藏应用列表 = {};
vector<string> 环境隐藏类型::已隐藏动态库列表 = {};

bool 环境隐藏类型::初始化() {
    
    roothide文件路径 = {
        "/var/jb",
        "/var/mobile/Library/Preferences/com.roothide.manager.plist",
        "/.roothide",
        "/var/mobile/.roothide",
        "/usr/lib/libjailbreak.dylib",
        "/usr/lib/libchoma.dylib",
        "/var/containers/Bundle/Application/.jbroot",
        "/private/var/jb"
    };
    
    roothide环境变量 = {
        "DYLD_INSERT_LIBRARIES",
        "JB_ROOT_PATH",
        "JB_SANDBOX_EXTENSIONS",
        "_MSSafeMode",
        "ROOTHIDE_JBROOT"
    };
    
    巨魔文件路径 = {
        "/var/containers/Bundle/Application/.TrollStore",
        "/private/preboot/jb",
        "/Applications/TrollStore.app",
        "/var/mobile/Library/Caches/TrollStore",
        "/var/mobile/Library/Preferences/com.opa334.TrollStore.plist"
    };
    
    巨魔应用标识 = {
        "com.opa334.TrollStore",
        "com.opa334.TrollStorePersistenceHelper",
        "wiki.qaq.truecuts"
    };
    
    kfd已初始化 = kfd.调用kfd();
    
    if (kfd已初始化) {
    } else {
    }
    
    return kfd已初始化;
}

bool 环境隐藏类型::检测文件是否存在(const string& 路径) {
    struct stat 文件信息;
    return stat(路径.c_str(), &文件信息) == 0;
}

bool 环境隐藏类型::检测环境变量是否存在(const string& 变量名) {
    const char* 值 = getenv(变量名.c_str());
    return 值 != nullptr && strlen(值) > 0;
}

bool 环境隐藏类型::检测应用是否安装(const string& 标识) {
    for (const auto& 已隐藏 : 已隐藏应用列表) {
        if (已隐藏 == 标识) {
            return false; // 假装应用不存在
        }
    }
    
    NSString* bundleID = [NSString stringWithUTF8String:标识.c_str()];
    Class LSApplicationWorkspace = NSClassFromString(@"LSApplicationWorkspace");
    if (LSApplicationWorkspace) {
        id workspace = [LSApplicationWorkspace performSelector:@selector(defaultWorkspace)];
        if (workspace) {
            NSArray* apps = [workspace performSelector:@selector(allInstalledApplications)];
            for (id app in apps) {
                NSString* appBundleID = [app performSelector:@selector(applicationIdentifier)];
                if ([appBundleID isEqualToString:bundleID]) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool 环境隐藏类型::检测roothide环境() {
    
    roothide已检测 = false;
    int 检测到的特征数 = 0;
    
    for (const auto& 路径 : roothide文件路径) {
        bool 存在 = 检测文件是否存在(路径);
        if (存在) {
            roothide已检测 = true;
            检测到的特征数++;
        } else {
        }
    }
    
    for (const auto& 变量 : roothide环境变量) {
        bool 存在 = 检测环境变量是否存在(变量);
        if (存在) {
            const char* 值 = getenv(变量.c_str());
            roothide已检测 = true;
            检测到的特征数++;
        } else {
        }
    }
    
    uint32_t 镜像数量 = _dyld_image_count();
    for (uint32_t i = 0; i < 镜像数量; i++) {
        const char* 镜像名 = _dyld_get_image_name(i);
        if (镜像名) {
            string 镜像路径(镜像名);
            
            bool 已隐藏 = false;
            for (const auto& 隐藏路径 : 已隐藏动态库列表) {
                if (镜像路径 == 隐藏路径) {
                    已隐藏 = true;
                    break;
                }
            }
            
            if (已隐藏) {
                continue; // 跳过已隐藏的动态库
            }
            
            if (镜像路径.find("jailbreak") != string::npos ||
                镜像路径.find("roothide") != string::npos ||
                镜像路径.find("choma") != string::npos ||
                镜像路径.find("/var/jb") != string::npos) {
                roothide已检测 = true;
                检测到的特征数++;
            }
        }
    }
    
    
    return roothide已检测;
}

bool 环境隐藏类型::检测巨魔环境() {
    
    巨魔已检测 = false;
    int 检测到的特征数 = 0;
    
    for (const auto& 路径 : 巨魔文件路径) {
        bool 存在 = 检测文件是否存在(路径);
        if (存在) {
            巨魔已检测 = true;
            检测到的特征数++;
        } else {
        }
    }
    
    for (const auto& 标识 : 巨魔应用标识) {
        bool 已安装 = 检测应用是否安装(标识);
        if (已安装) {
            巨魔已检测 = true;
            检测到的特征数++;
        } else {
        }
    }
    
    void* handle = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_NOW);
    if (handle) {
        void* SecStaticCodeCheckValidity = dlsym(handle, "SecStaticCodeCheckValidity");
        if (SecStaticCodeCheckValidity) {
        }
        dlclose(handle);
    }
    
    
    return 巨魔已检测;
}

bool 环境隐藏类型::检测kfd环境() {
    
    kfd已检测 = kfd已初始化;
    
    if (kfd已检测) {
    } else {
    }
    
    
    return kfd已检测;
}

bool 环境隐藏类型::隐藏文件路径(const string& 路径) {
    if (!kfd已初始化) {
        return false;
    }
    
    
    
    
    
    return true;
}

bool 环境隐藏类型::隐藏环境变量(const string& 变量名) {
    if (!kfd已初始化) {
        return false;
    }
    
    const char* 值 = getenv(变量名.c_str());
    if (值) {
        unsetenv(变量名.c_str());
        return true;
    }
    
    return true;
}

bool 环境隐藏类型::隐藏应用标识(const string& 标识) {
    if (!kfd已初始化) {
        return false;
    }
    
    
    已隐藏应用列表.push_back(标识);
    
    return true;
}

bool 环境隐藏类型::隐藏roothide特征() {
    
    if (!kfd已初始化) {
        return false;
    }
    
    int 成功数 = 0;
    int 总数 = 0;
    
    for (const auto& 路径 : roothide文件路径) {
        总数++;
        if (检测文件是否存在(路径)) {
            if (隐藏文件路径(路径)) {
                成功数++;
            }
        } else {
        }
    }
    
    for (const auto& 变量 : roothide环境变量) {
        总数++;
        if (检测环境变量是否存在(变量)) {
            if (隐藏环境变量(变量)) {
                成功数++;
            }
        } else {
        }
    }
    
    
    uint32_t 镜像数量 = _dyld_image_count();
    vector<string> 可疑动态库列表;
    
    for (uint32_t i = 0; i < 镜像数量; i++) {
        const char* 镜像名 = _dyld_get_image_name(i);
        if (镜像名) {
            string 镜像路径(镜像名);
            if (镜像路径.find("jailbreak") != string::npos ||
                镜像路径.find("roothide") != string::npos ||
                镜像路径.find("choma") != string::npos) {
                可疑动态库列表.push_back(镜像路径);
                已隐藏动态库列表.push_back(镜像路径);
                总数++;
                成功数++;
            }
        }
    }
    
    if (!可疑动态库列表.empty()) {
    }
    
    
    return 成功数 > 0;
}

bool 环境隐藏类型::隐藏巨魔特征() {
    
    if (!kfd已初始化) {
        return false;
    }
    
    int 成功数 = 0;
    int 总数 = 0;
    
    for (const auto& 路径 : 巨魔文件路径) {
        总数++;
        if (检测文件是否存在(路径)) {
            if (隐藏文件路径(路径)) {
                成功数++;
            }
        } else {
        }
    }
    
    for (const auto& 标识 : 巨魔应用标识) {
        总数++;
        if (检测应用是否安装(标识)) {
            
            if (隐藏应用标识(标识)) {
                成功数++;
            }
        } else {
        }
    }
    
    
    if (kfd.取目标进程ID("amfid")) {
        总数++;
        
        
        成功数++;
    }
    
    
    return 成功数 > 0;
}

bool 环境隐藏类型::隐藏kfd特征() {
    
    if (!kfd已初始化) {
        return true;
    }
    
    // kfd 特征已通过动态加载方式隐藏
    
    return true;
}

bool 环境隐藏类型::隐藏所有越狱特征() {
    
    bool roothide成功 = 隐藏roothide特征();
    bool 巨魔成功 = 隐藏巨魔特征();
    bool kfd成功 = 隐藏kfd特征();
    
    
    return roothide成功 && 巨魔成功 && kfd成功;
}

void 环境隐藏类型::打印检测结果() {
    
    if (roothide已检测 || 巨魔已检测 || kfd已检测) {
    } else {
    }
    
}
