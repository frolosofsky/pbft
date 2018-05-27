#pragma once

#include "pbft_types.h"

// There're mocks for digest and signature functions

inline Digest digest(Message::WriteOpRequest const &msg) {
    return (static_cast<Digest>(Message::Type::Write) << 60) + static_cast<Digest>(msg.value);
}

inline Digest digest(Message const &msg) {
    switch(msg.type) {
    case Message::Type::Write:
        return digest(msg.data.write);
    default:
        assert(not("Unreachable"));
    }
    return 0;
}

inline Digest digest(Message::OpMessage const &msg) {
    return digest(Message(Message::OpMessage(msg)));
}

inline Signature signature(Digest d, uintptr_t node) {
    return d + node / 2; // no overflow
}

inline Signature signature(Message const &msg, uintptr_t node) {
    return signature(digest(msg), node);
}

inline Digest recover_digest(Signature s, uintptr_t node) {
    return s - node / 2;
}

inline bool verify_digest(Message const &m, Digest d) {
    return digest(m) == d;
}

inline bool verify_message(Message const &m, Signature s, uintptr_t node) {
    return verify_digest(m, recover_digest(s, node));
}
