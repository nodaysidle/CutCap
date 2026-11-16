#pragma once

#include <QStringList>

namespace Cli {
bool shouldRunHeadless(const QStringList &args);
// Files to open in the GUI, from `cutcap open <file>...` or `cutcap <file>...`.
QStringList guiFiles(const QStringList &args);
int run(const QStringList &args);
}
