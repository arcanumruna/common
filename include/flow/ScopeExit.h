#pragma once

template <typename F>
class ScopeExit {
public:
    explicit ScopeExit(F&& f) noexcept : func(std::move(f)), active(true) {}
    ~ScopeExit() noexcept {
        if (active) func();
    }

    void dismiss() noexcept { active = false; }

private:
    F func;
    bool active;
};

template <typename F>
ScopeExit<F> make_scope_exit(F&& f) {
    return ScopeExit<F>(std::forward<F>(f));
}