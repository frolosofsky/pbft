#include "pbft.h"
#include "crypto.h"
#include <vector>

std::shared_ptr<Link> make_link(std::shared_ptr<Node> const &a, std::shared_ptr<Node> const &b) {
    auto link = Link::make(a, b);
    assert(link != nullptr);
    assert(a->has_link(b->id()));
    assert(b->has_link(a->id()));
    assert(Link::make(a, b) == nullptr);
    assert(Link::make(b, a) == nullptr);
    return link;
}

void links_test() {
    auto n1 = std::make_shared<Node>();
    auto n2 = std::make_shared<Node>();
    auto n3 = std::make_shared<Node>();

    assert(Link::make(n1, n1) == nullptr);
    assert(Link::make(n1, nullptr) == nullptr);
    auto link1 = make_link(n1, n2);
    auto link2 = make_link(n1, n3);
    auto link3 = make_link(n2, n3);

    auto n1_id = n1->id();
    n1.reset();
    assert(n2->has_link(n1_id));
    assert(n3->has_link(n1_id));
    assert(n2->has_link(n3->id()));
    assert(n3->has_link(n2->id()));

    link1.reset();
    assert(not(n2->has_link(n1_id)));
    link3.reset();
    assert(not(n2->has_link(n3->id())));
    assert(not(n3->has_link(n2->id())));
}

void messaging_test() {
    auto n1 = std::make_shared<Node>();
    auto n2 = std::make_shared<Node>();
    auto link = make_link(n1, n2);

    Message msg1(Message::WriteOpRequest{1});
    Message msg2(Message::WriteOpRequest{2});
    Message msg3(Message::WriteOpRequest{3});
    msg2.deliver_timeout = 2;
    msg3.deliver_timeout = 3;

    assert(not(Node::test_interface(*n1).send_to(n1->id(), Message::WriteOpRequest{0})));
    assert(Node::test_interface(*n1).send_to(n2->id(), std::move(msg3)));
    assert(Node::test_interface(*n1).send_to(n2->id(), std::move(msg1)));
    assert(Node::test_interface(*n1).send_to(n2->id(), std::move(msg2)));
    assert(Node::test_interface(*n2).send_to(n1->id(), Message::WriteOpRequest{42}));
    assert(Link::test_interface(*link).first().inbox.size() == 1);
    assert(Link::test_interface(*link).second().inbox.size() == 3);
    assert(Node::test_interface(*n1).inbox().size() == 0);
    assert(Node::test_interface(*n2).inbox().size() == 0);

    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 2);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(Node::test_interface(*n2).inbox().size() == 1);
    assert(Node::test_interface(*n1).inbox().back().second.data.write.value == 42);
    assert(Node::test_interface(*n2).inbox().back().second.data.write.value == 1);

    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 2);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(Node::test_interface(*n2).inbox().size() == 1);

    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 1);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(Node::test_interface(*n2).inbox().size() == 2);
    assert(Node::test_interface(*n2).inbox().back().second.data.write.value == 2);

    auto n2_id = n2->id();
    n2.reset();
    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 0);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(not(Node::test_interface(*n1).send_to(n2_id, Message::WriteOpRequest{42})));
    auto inbox = Node::test_interface(*n1).take_inbox();
    assert(inbox.size() == 1);
    assert(Node::test_interface(*n1).inbox().size() == 0);
}

void crypto_test() {
    auto node = std::make_shared<Node>();
    Message m(Message::WriteOpRequest{42});
    auto s = signature(m, node->id());
    assert(verify_message(m, s, node->id()));
}

struct ClientNode : Node {
    void on_tick() {
        broadcast(Message::WriteOpRequest{42});
    }
};

void pbft_state_f0_test() {
    State state(0); // f=0
    assert(not(state.prepare(0, 0)));
    assert(state.preprepare(0, 0));
    assert(state.state() == State::Type::PrePrepare);
    assert(not(state.preprepare(0, 0)));
    assert(not(state.prepare(1, 0)));
    assert(not(state.prepare(0, 1)));
    assert(state.prepare(0, 0));
    assert(state.approves() == 1);
    assert(state.state() == State::Type::Prepared);
    assert(not(state.prepare(0, 0)));
    assert(not(state.preprepare(0, 0)));
    assert(not(state.commit(1, 0)));
    assert(not(state.commit(0, 1)));
    assert(state.commit(0, 0));
    assert(state.state() == State::Type::Committed);
    assert(not(state.commit(0, 0)));
    assert(not(state.prepare(0, 0)));
    assert(not(state.preprepare(1, 0)));
    assert(not(state.preprepare(1, 1)));
    assert(state.preprepare(0, 1));
}

void pbft_state_f1_test() {
    State state(1); // f=1
    assert(not(state.prepare(0, 0)));
    assert(state.preprepare(0, 0));
    assert(state.state() == State::Type::PrePrepare);
    assert(not(state.preprepare(0, 0)));
    assert(not(state.prepare(1, 0)));
    assert(not(state.prepare(0, 1)));
    assert(state.prepare(0, 0));
    assert(state.approves() == 1);
    assert(state.state() == State::Type::Prepare);
    assert(state.prepare(0, 0));
    assert(state.approves() == 1);
    assert(state.state() == State::Type::Prepared);
    assert(not(state.prepare(0, 0)));
    assert(not(state.preprepare(0, 0)));
    assert(not(state.commit(1, 0)));
    assert(not(state.commit(0, 1)));
    assert(state.commit(0, 0));
    assert(state.state() == State::Type::Commit);
    assert(state.commit(0, 0));
    assert(state.state() == State::Type::Commit);
    assert(state.commit(0, 0));
    assert(state.state() == State::Type::Committed);
    assert(not(state.commit(0, 0)));
    assert(not(state.prepare(0, 0)));
    assert(not(state.preprepare(1, 0)));
    assert(not(state.preprepare(1, 1)));
    assert(state.preprepare(0, 1));
}

void pbft_messaging_f1_test() {
    std::vector<std::shared_ptr<PBFTNode>> nodes = {
        std::make_shared<PBFTNode>(PBFTNode::Role::Primary, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1)
    };
    for(auto &n : nodes)
        n->set_primary(nodes[0]);
    std::vector<std::shared_ptr<Link>> links;
    for(size_t i = 0; i < nodes.size() - 1; ++i) {
        for(size_t j = i + 1; j < nodes.size(); ++j) {
            links.emplace_back(make_link(nodes[i], nodes[j]));
        }
    }

    auto client = std::make_shared<ClientNode>();
    auto client_link = make_link(client, nodes[0]);

    auto tick_links = [&links, &client_link] {
        client_link->on_tick();
        for(auto &l : links)
            l->on_tick();
    };
    auto tick_nodes = [&nodes] {
        for(auto &n : nodes)
            n->on_tick();
    };

    client->on_tick();
    assert(nodes[0]->state().state() == State::Type::Init);
    tick_links();
    assert(nodes[0]->state().state() == State::Type::Init);
    assert(nodes[1]->state().state() == State::Type::Init);
    assert(nodes[2]->state().state() == State::Type::Init);
    assert(nodes[3]->state().state() == State::Type::Init);
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::PrePrepare);
    assert(nodes[1]->state().state() == State::Type::Init);
    assert(nodes[2]->state().state() == State::Type::Init);
    assert(nodes[3]->state().state() == State::Type::Init);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::PrePrepare);
    assert(nodes[1]->state().state() == State::Type::Prepare);
    assert(nodes[2]->state().state() == State::Type::Prepare);
    assert(nodes[3]->state().state() == State::Type::Prepare);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::Commit);
    assert(nodes[1]->state().state() == State::Type::Commit);
    assert(nodes[2]->state().state() == State::Type::Commit);
    assert(nodes[3]->state().state() == State::Type::Commit);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::Committed);
    assert(nodes[1]->state().state() == State::Type::Committed);
    assert(nodes[2]->state().state() == State::Type::Committed);
    assert(nodes[3]->state().state() == State::Type::Committed);
}

void pbft_messaging_f1_dead_node_test() {
    std::vector<std::shared_ptr<PBFTNode>> nodes = {
        std::make_shared<PBFTNode>(PBFTNode::Role::Primary, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1),
        std::make_shared<PBFTNode>(PBFTNode::Role::Replica, 1)
    };
    for(auto &n : nodes)
        n->set_primary(nodes[0]);
    std::vector<std::shared_ptr<Link>> links;
    for(size_t i = 0; i < nodes.size() - 1; ++i) {
        for(size_t j = i + 1; j < nodes.size(); ++j) {
            links.emplace_back(make_link(nodes[i], nodes[j]));
        }
    }

    nodes[1].reset(); // dead one

    auto client = std::make_shared<ClientNode>();
    auto client_link = make_link(client, nodes[0]);

    auto tick_links = [&links, &client_link] {
        client_link->on_tick();
        for(auto &l : links)
            l->on_tick();
    };
    auto tick_nodes = [&nodes] {
        for(auto &n : nodes)
            if(n != nullptr)
                n->on_tick();
    };

    client->on_tick();
    assert(nodes[0]->state().state() == State::Type::Init);
    tick_links();
    assert(nodes[0]->state().state() == State::Type::Init);
    assert(nodes[2]->state().state() == State::Type::Init);
    assert(nodes[3]->state().state() == State::Type::Init);
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::PrePrepare);
    assert(nodes[2]->state().state() == State::Type::Init);
    assert(nodes[3]->state().state() == State::Type::Init);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::PrePrepare);
    assert(nodes[2]->state().state() == State::Type::Prepare);
    assert(nodes[3]->state().state() == State::Type::Prepare);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::Commit);
    assert(nodes[2]->state().state() == State::Type::Commit);
    assert(nodes[3]->state().state() == State::Type::Commit);
    tick_links();
    tick_nodes();
    assert(nodes[0]->state().state() == State::Type::Committed);
    assert(nodes[2]->state().state() == State::Type::Committed);
    assert(nodes[3]->state().state() == State::Type::Committed);
}


int main() {
    links_test();
    messaging_test();
    crypto_test();
    pbft_state_f0_test();
    pbft_state_f1_test();
    pbft_messaging_f1_test();
    pbft_messaging_f1_dead_node_test();
    return 0;
}
