#pragma once

namespace NeoCore {

enum ErrorCode {
    Success = 0,

    GitTimeout      = 1001,
    GitNotFound     = 1002,
    GitCrash        = 1003,
    GitWriteError   = 1004,
    GitReadError    = 1005,

    ConfigParseFailed    = 2001,
    ConfigFormatUnknown  = 2002,
    ConfigMergeFailed    = 2003,

    WorkspaceNotInitialized = 3001,
    WorkspaceSyncFailed     = 3002,
    WorkspaceBranchNotFound = 3003,

    BuildFailed          = 4001,
    BuildExportFailed    = 4002,

    NetworkError         = 5001,
    DownloadFailed       = 5002,
    HashMismatch         = 5003,

    Cancelled            = 9000,
    Unknown              = 9999,
};

const char* ErrorCodeToString(ErrorCode code);

} // namespace NeoCore
