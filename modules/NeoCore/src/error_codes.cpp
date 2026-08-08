#include "error_codes.h"

namespace NeoCore {

const char* ErrorCodeToString(ErrorCode code)
{
    switch (code) {
        case Success:                return "操作成功";
        case GitTimeout:             return "Git 操作超时";
        case GitNotFound:            return "Git 未找到";
        case GitCrash:               return "Git 进程崩溃";
        case GitWriteError:          return "Git 写入错误";
        case GitReadError:           return "Git 读取错误";
        case ConfigParseFailed:      return "配置文件解析失败";
        case ConfigFormatUnknown:    return "未知配置文件格式";
        case ConfigMergeFailed:      return "配置合并失败";
        case WorkspaceNotInitialized: return "工作区未初始化";
        case WorkspaceSyncFailed:    return "工作区同步失败";
        case WorkspaceBranchNotFound: return "工作区分支未找到";
        case BuildFailed:            return "构建失败";
        case BuildExportFailed:      return "构建导出失败";
        case NetworkError:           return "网络错误";
        case DownloadFailed:         return "下载失败";
        case HashMismatch:           return "哈希校验不匹配";
        case Cancelled:              return "操作已取消";
        case Unknown:
        default:                     return "未知错误";
    }
}

} // namespace NeoCore
