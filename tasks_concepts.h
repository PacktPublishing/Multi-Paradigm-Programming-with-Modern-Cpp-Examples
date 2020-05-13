#pragma once

template<typename F> // function with no arguments
concept NullaryFunction = std::is_invocable<F>::value;

template<typename F, typename ArgumentType> // function with 1 argument
concept UnaryFunction = std::is_invocable<F, ArgumentType>::value;

// std::promise requires copy or move constructor, but also supports reference and void
template<typename T>
concept SupportsPromise = 
    std::is_copy_constructible<T>::value ||
    std::is_move_constructible<T>::value ||
    std::is_same<T, void>::value ||
    std::is_reference<T>::value;

