#pragma once

#include <utility>

// Wrap a value and provide value on request.
//
// Used to give unique named types and make code more readable.
//
// e.g.:
//
// WRAPPER(Verb, NameAndId);
// WRAPPER(Noun, NameAndId);
// WRAPPER(Details, NameAndId);
// ...
// struct Action {
//     Verb verb;
//     Noun noun;
//     Details details;
// };
//
// Even though all of Action's members are actually the same
// fundamental type, using a wrapper makes each unique so that when we
// created an instance of Action, we can't make the obvious mistakes.
#define WRAPPER(name, T) \
  class name \
  { \
    public: \
      using wrapped_type = T; \
      explicit name(T val) \
          : m_wrapped(std::move(val)) { \
      } \
  \
      name() = delete; \
  \
      auto val() const -> T { \
        return m_wrapped; \
      } \
  \
      auto ref() -> T& { \
        return m_wrapped; \
      } \
  \
      auto cref() const -> const T& { \
        return m_wrapped; \
      } \
  \
      auto operator&() -> T* { \
        return &m_wrapped; \
      } \
  \
      operator T() const { \
        return m_wrapped; \
      } \
  \
      operator T&() { \
        return m_wrapped; \
      } \
  \
    private: \
      T m_wrapped; \
  }

WRAPPER(foo, int);
