#pragma once

#include "pbft_types.h"

// There're mocks for digest and signature functions
// signature() signs message using node address as it being private key
// recover_digest() recovers digest using node address as public key
// high-end crypto lib, sort of

inline Digest digest(Message::WriteOpRequest const &msg) {
    return (static_cast<Digest>(Message::Type::Write) << 60) + static_cast<Digest>(msg.value);
}

inline Digest digest(Message::ReadOpRequest const &msg) {
    return (static_cast<Digest>(Message::Type::Read) << 60) + static_cast<Digest>(msg.index);
}

inline Digest digest(Message::WriteOpResponse const &msg) {
    return (static_cast<Digest>(Message::Type::WriteAck) << 60) + static_cast<Digest>(msg.index);
}

inline Digest digest(Message::ReadOpResponse const &msg) {
    return (static_cast<Digest>(Message::Type::ReadAck) << 60) + static_cast<Digest>(msg.value);
}

inline Digest digest(Message const &msg) {
    switch(msg.type) {
    case Message::Type::Write:
        return digest(msg.data.write);
    case Message::Type::Read:
        return digest(msg.data.read);
    case Message::Type::WriteAck:
        return digest(msg.data.write_ack);
    case Message::Type::ReadAck:
        return digest(msg.data.read_ack);
    default:
        std::cout << msg << std::endl;
        assert(not("Unreachable"));
    }
    return 0;
}

inline Digest digest(Message::OpRequestMessage const &msg) {
    return digest(Message(msg));
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
