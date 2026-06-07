#include "protocol/uci.h"
#include "core/attacks.h"

int main() {
    // All interaction goes through UCI protocol.
    // Run `./knightfall.exe` and type UCI commands.
    // Or point Arena/CuteChess at the executable.
    UCIProtocol uci;
    uci.run();
    return 0;
}
