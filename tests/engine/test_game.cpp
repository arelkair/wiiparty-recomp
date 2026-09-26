#include "wp/game.h"

namespace wp::game {

const Description& description() {
    static const Description kDescription = {"Engine tests", "engine_tests", "", "", "", nullptr, nullptr, 0};
    return kDescription;
}

}
