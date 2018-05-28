#pragma once

#include <memory>
#include <map>
#include <list>
#include <iostream>
#include <cassert>

using Digest = uint64_t;
using Signature = uint64_t;


// Common Message structure, includes all possible message types, both user and service ones

struct Message {
    enum class Type { Write, WriteAck, Read, ReadAck, Response, PrePrepare, Prepare, Commit };
    Type type;

    struct WriteOpRequest {
        int value;
    };
    struct WriteOpResponse { // IRL it'd be two different messages: ack and nack btw
        bool success;
        size_t index;
    };
    struct ReadOpRequest {
        size_t index;
    };
    struct ReadOpResponse {
        bool success;
        int value;
    };

    // Operational request. Might be encapsulated in Message.
    union OpRequestData {
        OpRequestData(WriteOpRequest const &msg) : write(msg) {}
        OpRequestData(WriteOpRequest &&msg) : write(std::move(msg)) {}
        OpRequestData(ReadOpRequest const &msg) : read(msg) {}
        OpRequestData(ReadOpRequest &&msg) : read(std::move(msg)) {}
        WriteOpRequest write;
        ReadOpRequest read;
    };
    struct OpRequestMessage {
        OpRequestMessage(WriteOpRequest const &msg) : type(Type::Write), data(msg) {}
        OpRequestMessage(WriteOpRequest &&msg) : type(Type::Write), data(std::move(msg)) {}
        OpRequestMessage(ReadOpRequest const &msg) : type(Type::Read), data(msg) {}
        OpRequestMessage(ReadOpRequest &&msg) : type(Type::Read), data(std::move(msg)) {}
        Type type;
        OpRequestData data;
    };

    // Operational response. Might be encapsulated in Message
    union OpResponseData {
        OpResponseData(WriteOpResponse &&msg) : write_ack(std::move(msg)) {}
        OpResponseData(ReadOpResponse &&msg) : read_ack(std::move(msg)) {}
        WriteOpResponse write_ack;
        ReadOpResponse read_ack;
    };
    struct OpResponseMessage {
        OpResponseMessage(WriteOpResponse &&msg) : type(Type::WriteAck), data(std::move(msg)) {}
        OpResponseMessage(ReadOpResponse &&msg) : type(Type::ReadAck), data(std::move(msg)) {}
        Type type;
        OpResponseData data;
    };
    struct Response {
        OpResponseMessage msg;
        Signature sig;
    };


    struct PrePrepare {
        OpRequestMessage msg;
        Signature sig;
        uintptr_t client;
        uint32_t view;
        uint32_t req_id;
    };

    struct Prepare : PrePrepare {
        Prepare(PrePrepare &&msg) : PrePrepare(std::move(msg)) {}
    };

    struct Commit : Prepare {
        Commit(Prepare &&msg) : Prepare(std::move(msg)) {}
    };

    union Data {
        Data(OpRequestMessage &&msg) {
            switch(msg.type) {
            case Type::Write:
                write = std::move(msg.data.write);
                break;
            case Type::Read:
                read = std::move(msg.data.read);
                break;
            case Type::WriteAck:
            case Type::ReadAck:
            case Type::Response:
            case Type::Prepare:
            case Type::PrePrepare:
            case Type::Commit:
                assert(not("Unreachable"));
            }
        }
        Data(OpResponseMessage &&msg) {
            switch(msg.type) {
            case Type::WriteAck:
                write_ack = std::move(msg.data.write_ack);
                break;
            case Type::ReadAck:
                read_ack = std::move(msg.data.read_ack);
                break;
            case Type::Write:
            case Type::Read:
            case Type::Response:
            case Type::Prepare:
            case Type::PrePrepare:
            case Type::Commit:
                assert(not("Unreachable"));
            }
        }
        Data(WriteOpRequest &&msg) : write(std::move(msg)) {}
        Data(ReadOpRequest &&msg) : read(std::move(msg)) {}
        Data(WriteOpResponse &&msg) : write_ack(std::move(msg)) {}
        Data(ReadOpResponse &&msg) : read_ack(std::move(msg)) {}
        Data(Response &&msg) : response(std::move(msg)) {}
        Data(PrePrepare &&msg) : preprepare(std::move(msg)) {}
        Data(Prepare &&msg) : prepare(std::move(msg)) {}
        Data(Commit &&msg) : commit(std::move(msg)) {}

        WriteOpRequest write;
        ReadOpRequest read;
        WriteOpResponse write_ack;
        ReadOpResponse read_ack;
        Response response;
        PrePrepare preprepare;
        Prepare prepare;
        Commit commit;
    };

    Message(WriteOpRequest &&msg) : type(Type::Write), data(std::move(msg)) {}
    Message(ReadOpRequest &&msg) : type(Type::Read), data(std::move(msg)) {}
    Message(WriteOpResponse &&msg) : type(Type::WriteAck), data(std::move(msg)) {}
    Message(ReadOpResponse &&msg) : type(Type::ReadAck), data(std::move(msg)) {}
    Message(OpRequestMessage &&msg) : type(msg.type), data(std::move(msg)) {}
    Message(OpRequestMessage const &msg) : type(msg.type), data(OpRequestMessage{msg}) {}
    Message(OpResponseMessage &&msg) : type(msg.type), data(std::move(msg)) {}
    Message(OpResponseMessage const &msg) : type(msg.type), data(OpResponseMessage{msg}) {}
    Message(Response &&msg) : type(Type::Response), data(std::move(msg)) {}
    Message(PrePrepare &&msg) : type(Type::PrePrepare), data(std::move(msg)) {}
    Message(Prepare &&msg) : type(Type::Prepare), data(std::move(msg)) {}
    Message(Commit &&msg) : type(Type::Commit), data(std::move(msg)) {}
    Message(Message&&) = default;
    Message(Message const &) = default;

    Data data;
    int deliver_timeout = 0; // in ticks
};

// Node::id() is a way to identify the node. IRL it might be ip address or so on

// Destroying of the node doesn't cause breaking its links. I.e. other ends still
// can (try) send messages to the dead node.

class Link;

class Node : public std::enable_shared_from_this<Node> {
public:
    virtual ~Node() = default;
    uintptr_t id() const;
    bool has_link(uintptr_t node) const;
    virtual void on_tick() {};

protected:
    auto take_inbox() { decltype(_inbox) inbox; std::swap(inbox, _inbox); return inbox; }
    bool unlink(uintptr_t node, bool interlink = true);
    bool send_to(uintptr_t node, Message &&msg);
    void broadcast(Message &&msg);

private:
    void link(uintptr_t node, std::shared_ptr<Link> const &link);
    void put(uintptr_t src_id, Message &&msg);

    std::map<uintptr_t, std::weak_ptr<Link>> _links;
    std::list<std::pair<uintptr_t, Message>> _inbox; // Messages are supposed to be processed in the next `on_tick`

public:
    struct test_interface {
        test_interface(Node &node) : _this(node) {}
        auto const &inbox() const { return _this._inbox; }
        auto const &links() const { return _this._links; }
        bool send_to(uintptr_t node, Message &&msg) { return _this.send_to(node, std::move(msg)); }
        auto take_inbox() { return _this.take_inbox(); }
        Node &_this;
    };
    friend class Link;
};


// The owner of link absraction. When destroyed, notifies nodes, and at this point
// them cannot send messages each other.

class Link{
public:
    static std::shared_ptr<Link> make(std::shared_ptr<Node> const &first, std::shared_ptr<Node> const &second);
    ~Link();
    void release(uintptr_t src_id);
    bool send(uintptr_t dst_id, Message &&msg);
    void on_tick();

private:
    Link(std::shared_ptr<Node> const &first, std::shared_ptr<Node> const &second) : first(first), second(second) {}
    struct Mailbox {
        Mailbox(std::shared_ptr<Node> const &node) : node_id(node->id()), node(node) {}
        uintptr_t node_id;
        std::weak_ptr<Node> node;
        std::list<Message> inbox; // messages in the link (channel, wire, whatever), not yet delivered to the `node`
    };
    struct Destinations {
        Mailbox &src, &dst;
    };

    // Represent corresponding Mailbox-es in source-destination way, where source.id == `id`
    Destinations get_dst(uintptr_t id);
    void unlink(Mailbox &a, Mailbox &b);
    void process_messages(uintptr_t src, Mailbox &dst);

    Mailbox first, second;

public:
    struct test_interface {
        test_interface(Link &link) : _this(link) {}
        Mailbox const &first() const { return _this.first; }
        Mailbox const &second() const { return _this.second; }
        Link &_this;
    };
};

template<typename Stream>
Stream &operator<<(Stream &os, Message::WriteOpRequest const &m) {
    return os << "value=" << m.value;
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::ReadOpRequest const &m) {
    return os << "index=" << m.index;
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::WriteOpResponse const &m) {
    return os << "success=" << m.success << ", index=" << m.index;
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::ReadOpResponse const &m) {
    return os << "success=" << m.success << ", value=" << m.value;
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::Response const &m) {
    return os << "sig=" << m.sig << ", " << Message(m.msg);
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::PrePrepare const &m) {
    return os << m.view << ":" << m.req_id << ", " << Message(m.msg);
}

template<typename Stream>
Stream &operator<<(Stream &os, Message const &m) {
    switch(m.type) {
    case Message::Type::Write:
        return os << "Write{" << m.data.write << "}";
    case Message::Type::Read:
        return os << "Read{" << m.data.read << "}";
    case Message::Type::WriteAck:
        return os << "WriteAck{" << m.data.write_ack << "}";
    case Message::Type::ReadAck:
        return os << "ReadAck{" << m.data.read_ack << "}";
    case Message::Type::Response:
        return os << "Response{" << m.data.response << "}";
    case Message::Type::PrePrepare:
        return os << "PrePrepare{" << m.data.preprepare << "}";
    case Message::Type::Prepare:
        return os << "Prepare{" << m.data.prepare << "}";
    case Message::Type::Commit:
        return os << "Commit{" << m.data.commit << "}";
    }
    return os; // happy gcc
}
