#include <utility>
#include <type_traits>

namespace cn::utils {
template<class F>
class scope_exit {
    F    _func;
    bool _active;

public:
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;

    template<class Fn>
    explicit scope_exit(Fn&& f) noexcept(std::is_nothrow_constructible_v<F, Fn&&>)
        : _func(std::forward<Fn>(f)), 
          _active(true) {}
          

    scope_exit(scope_exit&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : _func(std::move(other._func)), 
          _active(std::exchange(other._active, false)) {}


    ~scope_exit() noexcept {
        if (_active) {
            _func();
        }
    }

    void release() noexcept { 
        _active = false; 
    }
};

template<class F>
[[nodiscard]]
auto make_scope_exit(F&& f) {
    using Decayed = std::decay_t<F>;
    return scope_exit<Decayed>(std::forward<F>(f));
}
}
