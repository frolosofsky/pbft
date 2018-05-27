#pragma once

#include <memory>
#include <map>
#include <list>
#include <iostream>

using Digest = uint64_t;
using Signature = uint64_t;

struct Message {
    enum class Type { Write, PrePrepare, Prepare, Commit };
    Type type;

    struct WriteOpRequest {
        int value;
    };

    union OpData {
        OpData(WriteOpRequest const &msg) : write(msg) {}
        OpData(WriteOpRequest &&msg) : write(std::move(msg)) {}
        WriteOpRequest write;
    };

    struct OpMessage {
        OpMessage(WriteOpRequest const &msg) : type(Type::Write), data(msg) {}
        OpMessage(WriteOpRequest &&msg) : type(Type::Write), data(std::move(msg)) {}
        Type type;
        OpData data;
    };

    struct PrePrepare {
        OpMessage msg;
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
        Data(OpMessage &&msg) {
            switch(msg.type) {
            case Type::Write:
                write = std::move(msg.data.write);
                break;
            case Type::Prepare:
            case Type::PrePrepare:
            case Type::Commit:
                assert(not("Unreachable"));
            }
        }
        Data(WriteOpRequest &&msg) : write(std::move(msg)) {}
        Data(PrePrepare &&msg) : preprepare(std::move(msg)) {}
        Data(Prepare &&msg) : prepare(std::move(msg)) {}
        Data(Commit &&msg) : commit(std::move(msg)) {}

        WriteOpRequest write;
        PrePrepare preprepare;
        Prepare prepare;
        Commit commit;
    };

    Message(OpMessage &&msg) : type(msg.type), data(std::move(msg)) {}
    Message(OpMessage const &msg) : type(msg.type), data(OpMessage{msg}) {}
    Message(WriteOpRequest &&msg) : type(Type::Write), data(std::move(msg)) {}
    Message(PrePrepare &&msg) : type(Type::PrePrepare), data(std::move(msg)) {}
    Message(Prepare &&msg) : type(Type::Prepare), data(std::move(msg)) {}
    Message(Commit &&msg) : type(Type::Commit), data(std::move(msg)) {}
    Message(Message&&) = default;
    Message(Message const &) = default;

    Data data;
    int deliver_timeout = 0; // in ticks
};

// Node::id() is a way to identify the node. IRL it might be ip address or so on

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
    std::list<std::pair<uintptr_t, Message>> _inbox;

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

    // Represent corresponding Mailbox-es in source-destination way, source.id == `id`
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
    return os << m.value;
}

template<typename Stream>
Stream &operator<<(Stream &os, Message::PrePrepare const &m) {
    return os << m.view << ":" << m.req_id << ", " << Message(Message::OpMessage(m.msg));
}

template<typename Stream>
Stream &operator<<(Stream &os, Message const &m) {
    switch(m.type) {
    case Message::Type::Write:
        return os << "Write{" << m.data.write << "}";
    case Message::Type::PrePrepare:
        return os << "PrePrepare{" << m.data.preprepare << "}";
    case Message::Type::Prepare:
        return os << "Prepare{" << m.data.prepare << "}";
    case Message::Type::Commit:
        return os << "Commit{" << m.data.commit << "}";
    }
}
