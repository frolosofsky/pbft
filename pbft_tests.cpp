#include "pbft_types.h"

std::shared_ptr<Link> make_link(std::shared_ptr<Node> const &a, std::shared_ptr<Node> const &b) {
    auto link = a->link(b);
    assert(link != nullptr);
    assert(a->has_link(b->id()));
    assert(b->has_link(a->id()));
    assert(a->link(b) == nullptr);
    assert(b->link(a) == nullptr);
    return link;
}

void links_test() {
    auto n1 = std::make_shared<Node>();
    auto n2 = std::make_shared<Node>();
    auto n3 = std::make_shared<Node>();

    assert(n1->link(n1) == nullptr);
    assert(n1->link(nullptr) == nullptr);
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

    Message msg1(Message::UserMessage{1});
    Message msg2(Message::UserMessage{2});
    Message msg3(Message::UserMessage{3});
    msg2.deliver_timeout = 2;
    msg3.deliver_timeout = 3;

    assert(not(n1->send(n1->id(), Message::UserMessage{0})));
    assert(n1->send(n2->id(), std::move(msg3)));
    assert(n1->send(n2->id(), std::move(msg1)));
    assert(n1->send(n2->id(), std::move(msg2)));
    assert(n2->send(n1->id(), Message::UserMessage{42}));
    assert(Link::test_interface(*link).first().inbox.size() == 1);
    assert(Link::test_interface(*link).second().inbox.size() == 3);
    assert(Node::test_interface(*n1).inbox().size() == 0);
    assert(Node::test_interface(*n2).inbox().size() == 0);

    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 2);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(Node::test_interface(*n2).inbox().size() == 1);
    assert(Node::test_interface(*n1).inbox().back().second.data.u.payload == 42);
    assert(Node::test_interface(*n2).inbox().back().second.data.u.payload == 1);

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
    assert(Node::test_interface(*n2).inbox().back().second.data.u.payload == 2);

    auto n2_id = n2->id();
    n2.reset();
    link->on_tick();
    assert(Link::test_interface(*link).first().inbox.size() == 0);
    assert(Link::test_interface(*link).second().inbox.size() == 0);
    assert(Node::test_interface(*n1).inbox().size() == 1);
    assert(not(n1->send(n2_id, Message::UserMessage{42})));
}



int main() {
    links_test();
    messaging_test();
    return 0;
}
