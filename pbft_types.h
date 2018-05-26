#pragma once

#include <memory>
#include <map>
#include <list>

struct Message {
    enum class Type { Client, Internal };
    Type header;

    struct InternalMessage {
        int whatever;
        double foo;
    };

    struct UserMessage {
        int payload;
    };

    union Data {
        Data(InternalMessage &&msg) : i(std::move(msg)) {}
        Data(UserMessage &&msg) : u(std::move(msg)) {}
        InternalMessage i;
        UserMessage u;
    };

    Message(InternalMessage &&msg) : header(Type::Internal), data(std::move(msg)) {}
    Message(UserMessage &&msg) : header(Type::Client), data(std::move(msg)) {}
    Message(Message&&) = default;
    Message(Message const &) = default;

    Data data;
    int deliver_timeout = 0; // in ticks
};



// Node::id() is a way to identify the node. IRL it might be ip address or so on

class Link;

class Node : public std::enable_shared_from_this<Node> {
public:
    uintptr_t id() const;
    bool has_link(uintptr_t node) const;
    std::shared_ptr<Link> link(std::shared_ptr<Node> const &node);
    bool unlink(uintptr_t node, bool interlink = true);
    bool send(uintptr_t node, Message &&msg);

private:
    void link(uintptr_t node, std::shared_ptr<Link> const &link);
    void put(uintptr_t src_id, Message &&msg);

    std::map<uintptr_t, std::weak_ptr<Link>> _links;
    std::list<std::pair<uintptr_t, Message>> _inbox;
    friend class Link;

public:
    struct test_interface {
        test_interface(Node &node) : _this(node) {}
        auto const &inbox() const { return _this._inbox; }
        Node &_this;
    };
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
