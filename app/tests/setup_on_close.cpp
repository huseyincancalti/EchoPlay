#include <QString>
#include "SetupLock.h"

// tools/setup-live.sh: what EchoPlay does last when it closes with an update waiting (main.cpp). A tool of its own, so
// the product has no option that starts any program it is given.
int main(int argc, char *argv[])
{
    return argc == 2 && SetupLock::startSetup(QString::fromLocal8Bit(argv[1]), false) ? 0 : 1;
}
