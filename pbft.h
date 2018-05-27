#pragma once

#include "pbft_types.h"
#include "crypto.h"

class State {
public:
    enum class Type { Init, PrePrepare, Prepare, Prepared, Commit, Committed };

    State(int f = 1) : _f(f) {}

    Type state() const { return _state; }
    int approves() const { return _approves; }
    uint32_t view() const { return _view; }
    uint32_t req_id() const { return _req_id; }
    int f() const { return _f; }

    bool preprepare(uint32_t view, uint32_t req_id) {
        switch(_state) {
        case Type::Init:
            _view = view;
            _req_id = req_id;
            _approves = 1;
            _state = Type::PrePrepare;
            return true;
        case Type::PrePrepare:
        case Type::Prepare:
        case Type::Prepared:
        case Type::Commit:
        case Type::Committed:
            if(_view == view && _req_id == req_id - 1) {
                _view = view;
                _req_id = req_id;
                _approves = 1;
                _state = Type::PrePrepare;
                return true;
            }
            return false;
        }
    }

    bool prepare(uint32_t view, uint32_t req_id) {
        if(_view != view || _req_id != req_id)
            return false;
        switch(_state) {
        case Type::Init:
            return false;
        case Type::PrePrepare:
            _state = Type::Prepare;
            _approves = 0;
            [[clang::fallthrough]];
        case Type::Prepare:
            ++_approves;
            if(_approves >= _f * 2) {
                _state = Type::Prepared;
                _approves = 1;
            }
            return true;
        case Type::Prepared:
        case Type::Commit:
        case Type::Committed:
            return false;
        }
    }

    bool commit(uint32_t view, uint32_t req_id) {
        if(_view != view || _req_id != req_id)
            return false;
        switch(_state) {
        case Type::Init:
        case Type::PrePrepare:
        case Type::Prepare:
            return false;
        case Type::Prepared:
            _state = Type::Commit;
            _approves = 0;
            [[clang::fallthrough]];
        case Type::Commit:
            ++_approves;
            if(_approves >= _f * 2 + 1) {
                _state = Type::Committed;
                _approves = 1;
            }
            return true;
        case Type::Committed:
            return false;
        }
    }

private:
    Type _state = Type::Init;
    int _approves = 0;
    uint32_t _view, _req_id;
    int _f = 1;
};

class PBFTNode : public Node {
public:
    enum class Role { Primary, Replica };
    PBFTNode(Role r, int f): _state(f), _role(r) { assert (f > 0); }

    State const &state() const { return _state; }
    Role const &role() const { return _role; }
    void set_primary(std::shared_ptr<Node> const &p) { _primary = p; }

    void on_tick() override {
        auto inbox = take_inbox();
        for(auto &mm : inbox) {
            auto const s = mm.first;
            auto &m = mm.second;
            std::cout << id() << " <- " << m << std::endl;
            switch(m.type) {
            case Message::Type::Write:
                process(s, std::move(m.data.write));
                break;
            case Message::Type::PrePrepare:
                process(s, std::move(m.data.preprepare));
                break;
            case Message::Type::Prepare:
                process(s, std::move(m.data.prepare));
                break;
            case Message::Type::Commit:
                process(s, std::move(m.data.commit));
                break;
            }
        }
    }

private:
    auto prepreare(uintptr_t client, Message::WriteOpRequest &&msg) const {
        auto sig = signature(digest(msg), id());
        return Message::PrePrepare{std::move(msg), sig, client, _view, req_id()};
    }

    auto prepare(Message::PrePrepare &&msg) const {
        return Message::Prepare(std::move(msg));
    }

    auto commit(Message::Prepare &&msg) const {
        return Message::Commit(std::move(msg));
    }

    void process(uintptr_t sender, Message::WriteOpRequest &&msg) {
        if(_role != Role::Primary)
            return; // only primary reacts on client requests. Change-view in TODO
        auto p = prepreare(sender, std::move(msg));
        if(_state.preprepare(p.view, p.req_id))
            broadcast(std::move(p));
    }

    template<typename T>
    bool verify_message(T const &msg) {
        auto ptr = _primary.lock();
        return ptr != nullptr && ::verify_message(msg.msg, msg.sig, ptr->id());
    }

    void process(uintptr_t sender[[gnu::unused]], Message::PrePrepare &&msg) {
        if(_role == Role::Primary)
            return; // only replicas react on preprepare
        if(!verify_message(msg))
            return;
        if(_state.preprepare(msg.view, msg.req_id) && _state.prepare(msg.view, msg.req_id))
            broadcast(prepare(std::move(msg)));
    }

    void process(uintptr_t sender[[gnu::unused]], Message::Prepare &&msg) {
        if(!verify_message(msg))
            return;
        if(_state.prepare(msg.view, msg.req_id) && _state.commit(msg.view, msg.req_id))
            broadcast(commit(std::move(msg)));
    }

    void process(uintptr_t sender[[gnu::unused]], Message::Commit &&msg) {
        if(!verify_message(msg))
            return;
        if(_state.commit(msg.view, msg.req_id) && _state.state() == State::Type::Committed) {
            std::cout << id() << " Complete!" << std::endl;
        }
    }

    uint32_t req_id() const {
        static uint32_t id = 0;
        return ++id;
    }

    State _state;
    Role _role = Role::Replica;
    uint32_t _view = 0;
    std::weak_ptr<Node> _primary;
};
