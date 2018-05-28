#pragma once

#include <cassert>
#include "pbft_types.h"
#include "crypto.h"

#if defined (__clang__)
#define FALLTHROUGH [[clang::fallthrough]]
#else
#define FALLTHROUGH
#endif

// Represents state of the pbft node.
// Has few hacky fallthrough-s in order to cover f=0

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
        return false; // happy gcc
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
            FALLTHROUGH;
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
        return false; // happy gcc
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
            FALLTHROUGH;
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
        return false; // happy gcc
    }

private:
    Type _state = Type::Init;
    int _approves = 0;
    uint32_t _view, _req_id;
    int _f = 1;
};


// The pbft node. Doesn't support change-view. You have to say which node is primary.
// View is harcoded and 0.
// No way to restore from broken (stuck) state.
// Doesn't work with f=0, seems to require additional internal hops. Or `State` handles
// it in a wrong way.

// After node handles user message it signs it by its private key (node->id())
// Maybe we need to resign it after every hop? Or sign by user?

class PBFTNode : public Node {
public:
    enum class Role { Primary, Replica };

    struct SuccessStrategy {
        virtual ~SuccessStrategy() = default;
        virtual Message::OpResponseMessage accept(Message::OpRequestMessage const &msg) = 0;
    };
    using SuccessStrategyPtr = std::unique_ptr<SuccessStrategy>;

    PBFTNode(Role r, int f): _state(f), _role(r) { assert (f > 0); }

    State const &state() const { return _state; }
    Role const &role() const { return _role; }
    void set_primary(std::shared_ptr<Node> const &p) { _primary = p; }
    void set_success_startegy(SuccessStrategyPtr &&s) { _success_strategy = std::move(s); }

    void on_tick() override {
        auto inbox = take_inbox();
        for(auto &mm : inbox) {
            auto const s = mm.first;
            auto &m = mm.second;
            switch(m.type) {
            case Message::Type::Write:
                process(s, std::move(m.data.write));
                break;
            case Message::Type::Read:
                process(s, std::move(m.data.read));
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
            case Message::Type::WriteAck:
            case Message::Type::ReadAck:
            case Message::Type::Response:
                assert(not("Unreachable"));
            }
        }
    }

private:
    template<typename T>
    bool verify_message(T const &msg) {
        auto ptr = _primary.lock();
        return ptr != nullptr && ::verify_message(msg.msg, msg.sig, ptr->id());
    }

    template<typename T>
    auto prepreare(uintptr_t client, T &&msg) const {
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

    void process(uintptr_t sender, Message::ReadOpRequest &&msg) {
        if(_role != Role::Primary)
            return; // only primary reacts on client requests. Change-view in TODO
        auto p = prepreare(sender, std::move(msg));
        if(_state.preprepare(p.view, p.req_id))
            broadcast(std::move(p));
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
        if(_state.commit(msg.view, msg.req_id) && _state.state() == State::Type::Committed)
            success(msg.client, msg.msg);
    }

    void success(uintptr_t client, Message::OpRequestMessage const &msg) {
        if(_success_strategy == nullptr)
            return;
        auto answer = _success_strategy->accept(msg);
        auto sig = signature(digest(answer), id());
        send_to(client, Message::Response{std::move(answer), sig});
    }

    uint32_t req_id() const {
        static uint32_t id = 0;
        return ++id;
    }

    State _state;
    Role _role = Role::Replica;
    uint32_t _view = 0;
    std::weak_ptr<Node> _primary;
    SuccessStrategyPtr _success_strategy;
};
