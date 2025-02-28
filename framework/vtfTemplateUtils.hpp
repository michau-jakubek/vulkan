#ifndef __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
#define __VTF_TEMPLATE_UTILS_HPP_INCLUDED__

#include "vtfVector.hpp"

namespace vtf
{

template<class T_In, class T_Out>
void transformDistance (T_In inMin, T_In inMax, T_In in, T_Out outMin, T_Out outMax, add_ref<T_Out> out, bool mirror)
{
	T_In lsrc = std::abs(make_signed(inMax) - make_signed(inMin));
	T_Out ldst = T_Out(std::abs(make_signed(outMax) - make_signed(outMin)));
	T_In sdist = std::abs(make_signed(in) - make_signed(inMin));
	T_Out ddist = T_Out((double(sdist) * ldst) / lsrc);
	out = mirror ? (outMax - ddist) : (outMin + ddist);
}

template<class X, size_t N>
VecX<X, N> barycenter (
	const VecX<X, N>& a,
	const VecX<X, N>& b,
	const VecX<X, N>& c)
{
	VecX<X, N> p;
	p.x(X((a.x() + b.x() + c.x()) / 3.0));
	p.y(X((a.y() + b.y() + c.y()) / 3.0));
	if constexpr (N == 3)
	{
		p.z(X((a.z() + b.z() + c.z()) / 3.0));
	}
	return p;
}

template<class X>
VecX<X, 2> barycenter (
	const VecX<X, 2>& a,
	const VecX<X, 2>& b,
	const VecX<X, 2>& c,
	const VecX<X, 2>& d)
{
	return VecX<X, 2>(
		X((a.x() + b.x() + c.x() + d.x()) / 4.0),
		X((a.y() + b.y() + c.y() + d.y()) / 4.0)
	);
}

namespace enumerator
{

template<typename X>
class Enumerator {
public:
    using value_type = typename X::value_type;
    Enumerator (X& container) : m_container(container) {}

    class Iterator {
    public:
        Iterator (typename X::iterator it, size_t index) : m_it(it), m_index(index) {}

        std::pair<size_t, add_ref<value_type>> operator*() {
            return { m_index, *m_it };
        }

        Iterator& operator++ () {
            ++m_it;
            ++m_index;
            return *this;
        }

        bool operator!= (const Iterator& other) const {
            return m_it != other.m_it;
        }

    private:
        typename X::iterator    m_it;
        size_t                  m_index;
    };

    Iterator begin () {
        return Iterator(m_container.begin(), 0);
    }

    Iterator end () {
        return Iterator(m_container.end(), m_container.size());
    }

private:
    X& m_container;
};

template<typename X>
Enumerator<X> enumerate (X& container) { return Enumerator<X>(container); }

} // namespace enumerator

namespace subset
{

template<class UserData> struct Subset {
	uint32_t start_index;
	uint32_t length;
	UserData user_data;
	Subset () : start_index(0), length(0), user_data() {}
	Subset (uint32_t start, uint32_t len) : start_index(start), length(len), user_data() {}
	Subset (uint32_t start, uint32_t len, UserData user) : start_index(start), length(len), user_data(user) {}
};
template<class UserData> bool areDisjoint (add_cref<Subset<UserData>> a, add_cref<Subset<UserData>>b) {
	return (a.start_index + a.length <= b.start_index) || (b.start_index + b.length <= a.start_index);
}
template<class UserData> bool checkSubsetsDisjoint (add_cref<std::vector<Subset<UserData>>> subsets) {
	for (auto i = 0u; i < subsets.size(); ++i) {
		for (auto j = i + 1u; j < subsets.size(); ++j) {
			if (!areDisjoint<UserData>(subsets[i], subsets[j])) {
				return false;
			}
		}
	}
	return true;
}

} // namespace subset

} // namesapce vtf

#endif // __VTF_TEMPLATE_UTILS_HPP_INCLUDED__
