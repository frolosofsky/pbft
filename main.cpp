#include "pbft.h"
#include <vector>


// Database built on top of PBFT

class PBFT_DB : public PBFTNode::SuccessStrategy {
    Message::OpResponseMessage accept(Message::OpRequestMessage const &msg) override {
        switch(msg.type) {
        case Message::Type::Write:
            return accept(msg.data.write);
        case Message::Type::Read:
            return accept(msg.data.read);
        case Message::Type::WriteAck:
        case Message::Type::ReadAck:
        case Message::Type::Response:
        case Message::Type::PrePrepare:
        case Message::Type::Prepare:
        case Message::Type::Commit:
            assert(not("Unreachable"));
        }
    }

    Message::WriteOpResponse accept(Message::WriteOpRequest const &msg) {
        _data.emplace_back(msg.value);
        return Message::WriteOpResponse{true, _data.size() - 1};
    }

    Message::ReadOpResponse accept(Message::ReadOpRequest const &msg) {
        int value = 0;
        bool success = false;
        if(msg.index < _data.size()) {
            value = _data[msg.index];
            success = true;
        }
        return Message::ReadOpResponse{success, value};
    }

    std::vector<int> _data;
};


class ClientNode : public Node {
public:
    void action(Message::OpRequestMessage &&msg, int answers) {
        std::cout << "Send " << msg << std::endl;
        broadcast(std::move(msg)); // don't care. only primary node should process it
        _actual_answers = 0;
        _expected_answers = answers;
    }

    bool ready() const {
        return _expected_answers == _actual_answers;
    }

    void on_tick() override {
        auto inbox = take_inbox();
        for(auto const &m : inbox) {
            switch(m.second.type) {
            case Message::Type::Response: {
                auto const &r = m.second.data.response;
                char const *verified = (verify_message(r.msg, r.sig, m.first) ? "Verified" : "Malformed");
                std::cout << m.first << " -> " << m.second << " :: " << verified  << std::endl;
                ++_actual_answers;
            } break;
            case Message::Type::Write:
            case Message::Type::Read:
            case Message::Type::WriteAck:
            case Message::Type::ReadAck:
                assert(not("Unreachable"));
            case Message::Type::PrePrepare:
            case Message::Type::Prepare:
            case Message::Type::Commit:
                // Client is interconnected with all nodes, here you can debug service
                // messages comming from nodes
                // std::cout << m.first << " -> " << m.second << std::endl;
                break;
            }
        }
    }

private:
    int _expected_answers = 0;
    int _actual_answers = 0;
};


class Simulator {
public:
    using Action = Message::OpRequestMessage;

    Simulator(int f, int nodes = 0) {
        nodes = std::max(nodes, 3 * f + 1);
        init_nodes(f, nodes);
    }

    void run() {
        constexpr int ticks_limit = 10000;
        int c = 0;
        while(not(_actions.size() == 0 && _client->ready()) && c < ticks_limit) {
            tick();
            ++c;
        }
        std::cout << "Simulation has taken " << c << " ticks" << std::endl;
    }

    void actions(std::list<Action> &&a) {
        _actions = std::move(a);
    }

    void destroy_node(size_t index) {
        assert(index < _nodes.size());
        _nodes[index].reset();
    }

private:
    void init_nodes(int f, int n) {
        _client = std::make_shared<ClientNode>();
        for(int i = 0; i < n; ++i) {
            _nodes.emplace_back(std::make_shared<PBFTNode>(i == 0 ? PBFTNode::Role::Primary : PBFTNode::Role::Replica, f));
            _nodes[i]->set_primary(_nodes[0]);
            _nodes[i]->set_success_startegy(std::make_unique<PBFT_DB>());
            _links.emplace_back(Link::make(_client, _nodes[i]));
        }
        for(size_t i = 0; i < _nodes.size() - 1; ++i) {
            for(size_t j = i + 1; j < _nodes.size(); ++j) {
                _links.emplace_back(Link::make(_nodes[i], _nodes[j]));
            }
        }

    }

    int alive_nodes() {
        int c = 0;
        for(auto const &n : _nodes)
            if(n != nullptr)
                ++c;
        return c;
    }

    void tick() {
        for(auto &l : _links)
            l->on_tick();
        for(auto &n : _nodes)
            if(n != nullptr)
                n->on_tick();
        if(_client->ready() && _actions.size() > 0) {
            _client->action(std::move(_actions.front()), alive_nodes());
            _actions.erase(_actions.begin());
        }
        _client->on_tick();
    }

    std::shared_ptr<ClientNode> _client;
    std::vector<std::shared_ptr<PBFTNode>> _nodes;
    std::vector<std::shared_ptr<Link>> _links;
    std::list<Action> _actions;
};


int main() {
    Simulator sim(1);
    sim.actions({
        Message::WriteOpRequest{1},
        Message::WriteOpRequest{2},
        Message::WriteOpRequest{10},
        Message::ReadOpRequest{0},
        Message::ReadOpRequest{2},
        Message::ReadOpRequest{3},
        });
    sim.run();
    sim.destroy_node(1);
    sim.actions({
        Message::WriteOpRequest{1000},
        Message::WriteOpRequest{1234},
        Message::WriteOpRequest{9876},
        Message::ReadOpRequest{5},
        Message::ReadOpRequest{10},
        Message::ReadOpRequest{3},
        });
    sim.run();
    return 0;
}
