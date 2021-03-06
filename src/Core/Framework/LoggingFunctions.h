/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 */

#pragma once

#include <QString>
#include "CoreTypes.h"

/// Specifies the different available log levels. Each log level includes all the output from the levels above it.
enum LogChannel
{
    LogChannelError = 1,
    LogChannelWarning = 2,
    LogChannelInfo = 4,
    LogChannelDebug = 8,

    // The following are bit combinations of the above primitive channels:
    // Do not use these in calls to PrintLogMessage, but only in SetEnabledLogChannels.

    LogLevelQuiet = 0, ///< Disable all output logging.
    LogLevelErrorsOnly = LogChannelError,
    LogLevelErrorWarning = LogChannelError | LogChannelWarning,
    LogLevelErrorWarnInfo = LogChannelError | LogChannelWarning | LogChannelInfo,
    LogLevelErrorWarnInfoDebug = LogChannelError | LogChannelWarning | LogChannelInfo | LogChannelDebug
};

/// Outputs a message to the log to the given channel.
void PrintLogMessage(u32 logChannel, const char *str);
/// Returns true if the given log channel is enabled.
bool IsLogChannelEnabled(u32 logChannel);

static void LogError(const std::string &msg)    { if (IsLogChannelEnabled(LogChannelError)) PrintLogMessage(LogChannelError, ("Error: " + msg + "\n").c_str());                 }
static void LogWarning(const std::string &msg)  { if (IsLogChannelEnabled(LogChannelWarning)) PrintLogMessage(LogChannelWarning, ("Warning: " + msg + "\n").c_str());               }
static void LogInfo(const std::string &msg)     { if (IsLogChannelEnabled(LogChannelInfo)) PrintLogMessage(LogChannelInfo, (msg + "\n").c_str());                             }
static void LogDebug(const std::string &msg)    { if (IsLogChannelEnabled(LogChannelDebug)) PrintLogMessage(LogChannelDebug, ("Debug: " + msg + "\n").c_str());                 }

static void LogError(const char *msg)    { if (IsLogChannelEnabled(LogChannelError)) PrintLogMessage(LogChannelError, (std::string("Error: ") + msg + "\n").c_str());           }
static void LogWarning(const char *msg)  { if (IsLogChannelEnabled(LogChannelWarning)) PrintLogMessage(LogChannelWarning, (std::string("Warning: ") + msg + "\n").c_str());         }
static void LogInfo(const char *msg)     { if (IsLogChannelEnabled(LogChannelInfo)) PrintLogMessage(LogChannelInfo, (std::string(msg) + "\n").c_str());                       }
static void LogDebug(const char *msg)    { if (IsLogChannelEnabled(LogChannelDebug)) PrintLogMessage(LogChannelDebug, (std::string("Debug: ") + msg + "\n").c_str());           }

///\todo UTF-8 -enable the following.

static void LogError(const QString &msg)    { if (IsLogChannelEnabled(LogChannelError)) PrintLogMessage(LogChannelError, ("Error: " + msg + "\n").toStdString().c_str());       }
static void LogWarning(const QString &msg)  { if (IsLogChannelEnabled(LogChannelWarning)) PrintLogMessage(LogChannelWarning, ("Warning: " + msg + "\n").toStdString().c_str());     }
static void LogInfo(const QString &msg)     { if (IsLogChannelEnabled(LogChannelInfo)) PrintLogMessage(LogChannelInfo, (msg + "\n").toStdString().c_str());                   }
static void LogDebug(const QString &msg)    { if (IsLogChannelEnabled(LogChannelDebug)) PrintLogMessage(LogChannelDebug, ("Debug: " + msg + "\n").toStdString().c_str());       }
