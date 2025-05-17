## Wspólne definicje i makra
W tym rozdziale zostaną omówione/pokazane definicje typów i makrodefinicje
używane w całym projekcie. Wiele z nich zostało skopiowanych ze strzępków
zamierzchłych projektów, które nigdy nie wyszły z garażu, nie mniej jednak
na przestrzeni lat okazały się użyteczne i zostały należycie przetestowane.

### Modifikatory typów
```cpp
#include "vtfZDeletable.hpp"
template<class C> using add_cst    = typename std::add_const<C>::type;
template<class P> using add_ptr    = typename std::add_pointer<P>::type;
template<class R> using add_ref    = typename std::add_lvalue_reference<R>::type;
template<class R> using add_rref   = typename std::add_rvalue_reference<R>::type;
template<class P> using add_cptr   = add_ptr<add_cst<P>>;
template<class R> using add_cref   = add_ref<add_cst<R>>;
template<class X> struct add_extent	{ typedef X type[]; };
```

### Typy złożone
```cpp
#include "vtfCUtils.hpp"
typedef std::vector<std::string> vtf::strings;
typedef std::map<std::string, std::string> vtf::string_to_string_map;
```
