#include "pbft_types.h"


uintptr_t Node::id() const {
    return reinterpret_cast<uintptr_t>(this);
}

std::shared_ptr<Link> Node::link(std::shared_ptr<Node> const &node) {
    return Link::make(shared_from_this(), node);
}

void Node::link(uintptr_t node, std::shared_ptr<Link> const &link) {
    auto ins = _links.emplace(node, link);
    assert(ins.second);
}

bool Node::has_link(uintptr_t node) const {
    return _links.find(node) != _links.end();
}

bool Node::unlink(uintptr_t node, bool release_link) {
    auto it = _links.find(node);
    if(it == _links.end())
        return false;
    if(release_link) {
        auto link_ptr = it->second.lock();
        assert(link_ptr != nullptr);
        link_ptr->release(id());
    }
    _links.erase(it);
    return true;
}

bool Node::send(uintptr_t node, Message &&msg) {
    auto it = _links.find(node);
    if(it == _links.end())
        return false;
    auto link_ptr = it->second.lock();
    assert(link_ptr != nullptr);
    return link_ptr->send(node, std::move(msg));
}

void Node::put(uintptr_t src_id, Message &&msg) {
    assert(has_link(src_id));
    _inbox.push_back({src_id, std::move(msg)});
}


Link::Destinations Link::get_dst(uintptr_t id) {
    if(first.node_id == id) {
        return {first, second};
    } else if (second.node_id == id) {
        return {second, first};
    } else {
        assert(not("Unreachable"));
    }
    return {first, second};
}

std::shared_ptr<Link> Link::make(std::shared_ptr<Node> const &first, std::shared_ptr<Node> const &second) {
    if(first == nullptr || second == nullptr || first == second || first->has_link(second->id()) || second->has_link(first->id()))
        return nullptr;
    std::shared_ptr<Link> ptr(new Link(first, second)); // std::make_shared requires public c-tor
    first->link(second->id(), ptr);
    second->link(first->id(), ptr);
    return ptr;
}

Link::~Link() {
    unlink(first, second);
    unlink(second, first);
}

void Link::release(uintptr_t src_id) {
    auto d = get_dst(src_id);
    assert(d.src.node_id == src_id);
    d.src.node.reset();
    auto dst_node = d.dst.node.lock();
    if(dst_node != nullptr)
        dst_node->unlink(src_id);
}

void Link::unlink(Mailbox &a, Mailbox &b) {
    auto a_ptr = a.node.lock();
    if(a_ptr != nullptr)
        a_ptr->unlink(b.node_id, false);
}

bool Link::send(uintptr_t dst_id, Message &&msg) {
    auto d = get_dst(dst_id);
    assert(d.src.node_id == dst_id);
    if(d.src.node.expired())
        return false; // just drop the message
    d.src.inbox.push_back(std::move(msg));
    return true;
}

void Link::on_tick() {
    process_messages(second.node_id, first);
    process_messages(first.node_id, second);
}

void Link::process_messages(uintptr_t src, Mailbox &dst) {
    for(auto it = dst.inbox.begin(); it != dst.inbox.end(); ) {
        if(it->deliver_timeout > 0) {
            --it->deliver_timeout;
            ++it;
        } else {
            auto node_ptr = dst.node.lock();
            if(node_ptr == nullptr) {
                dst.inbox.clear();
                return;
            }
            node_ptr->put(src, std::move(*it));
            it = dst.inbox.erase(it);
        }
    }
}
