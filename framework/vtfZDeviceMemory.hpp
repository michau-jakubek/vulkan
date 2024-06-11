#ifndef __VTF_ZDEVICEMEMORY_HPP_INCLUDED__
#define __VTF_ZDEVICEMEMORY_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"

#include <deque>
#include <iterator>


namespace vtf
{

uint32_t	findMemoryTypeIndex	(ZDevice device, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
auto		createMemory		(ZDevice device, add_cref<VkMemoryRequirements> requirements,
								VkMemoryPropertyFlags properties, VkDeviceSize desiredSize, bool sparse) -> std::vector<ZDeviceMemory>;
auto		mapMemory			(ZDeviceMemory memory) -> add_ptr<uint8_t>;
void		unmapMemory			(ZDeviceMemory memory);
void		flushMemory			(ZDeviceMemory memory);
void		invalidateMemory	(ZDeviceMemory memory);


/*
*	extern ZBuffer buffer;
*	Alloc alloc(buffer);
*	auto end = alloc.end<int>();
*	for (auto i = alloc.begin<int>(); i != end; ++i) {
*		std::cout << *i << std::endl;
*	}
*/
struct Alloc
{
	struct Impl;
	template<class> struct Iter;

	friend struct Impl;
	template<class> friend struct Iter;

	template<class X> Iter<X> begin	();
	template<class X> Iter<X> end	();

	Alloc (ZBuffer buffer);
	Alloc (add_cref<std::vector<ZDeviceMemory>> allocs);
	virtual ~Alloc ();

	add_ptr<uint8_t>	getMemory	(uint32_t index);
	long				getTotal	(std::size_t elementSize) const;

protected:
	add_cref<std::vector<ZDeviceMemory>>	m_allocs;
	std::map<uint32_t, add_ptr<uint8_t>>	m_map;
	const uint32_t							m_count;
	VkDeviceSize							m_total;
};

struct Alloc::Impl
{
	struct Value
	{
		add_ptr<uint8_t>	ptr1;
		add_ptr<uint8_t>	ptr2;
		uint32_t			len1;
		uint32_t			len2;
	};

	void	inc		();
	void	dec		();
	Value	val		();
	long	diff	(add_cref<Impl> other) const;
	bool	eq		(add_cref<Impl> other) const;
	void	verify	(add_cref<Impl> other) const;
	void	assign	(add_cref<Impl> other);

	Impl(add_ref<Alloc> alloc, long pos, uint32_t size);

protected:
	add_ref<Alloc>		m_alloc;
	long				m_pos;
	uint32_t			m_size;
};

template<class X> struct _alloc_iter_value
{
	typedef typename Alloc::Impl::Value Value;
	uint8_t fakeValue[sizeof(X)];
	Value	maybeValue;

	_alloc_iter_value(add_cref<Value> value) : fakeValue(), maybeValue(value) {}
	operator add_ref<X>() {
		if (nullptr == maybeValue.ptr2)
			return *reinterpret_cast<add_ptr<X>>(maybeValue.ptr1);
		else
		{
			std::copy_n(maybeValue.ptr1, maybeValue.len1, std::begin(fakeValue));
			std::copy_n(maybeValue.ptr2, maybeValue.len2, std::next(std::begin(fakeValue), maybeValue.len1));
		}
		return *reinterpret_cast<add_ptr<X>>(fakeValue);
	}
	add_ref<X> operator ()() { return *this; }
	add_ref<_alloc_iter_value> operator= (add_cref<X> x) {
		if (nullptr == maybeValue.ptr2)
			*reinterpret_cast<add_ptr<X>>(maybeValue.ptr1) = x;
		else
		{
			*reinterpret_cast<add_ptr<X>>(fakeValue) = x;
			std::copy_n(std::begin(fakeValue), maybeValue.len1, maybeValue.ptr1);
			std::copy_n(std::next(std::begin(fakeValue), maybeValue.len1), maybeValue.len2, maybeValue.ptr2);
		}
		return *this;
	}
};

template<class X>
struct Alloc::Iter : Alloc::Impl
{
	using iterator_category	= std::random_access_iterator_tag;
	using value_type		= X;
	using difference_type	= long;
	using pointer			= add_ptr<void>;
	using reference			= _alloc_iter_value<X>;

	Iter (add_ref<Alloc> alloc, long pos, uint32_t size)
		: Impl(alloc, pos, size) {}

	difference_type	operator-	(add_cref<Iter> other) const	{ return diff(other); }
	add_ref<Iter>	operator++	()								{ inc(); return *this; }
	add_ref<Iter>	operator++	(int)							{ dec(); return *this; }
	reference		operator*	()								{ return _alloc_iter_value<X>(val()); }
	reference		operator	()()							{ return _alloc_iter_value<X>(val()); }
	bool			operator!=	(add_cref<Iter> other) const	{ return !eq(other); }
	add_ref<Iter>	operator=	(add_cref<Iter> other)			{ assign(other); return *this; }
};

template<class X> Alloc::Iter<X> Alloc::begin() { return Iter<X>(*this, 0, static_cast<uint32_t>(sizeof(X))); }
template<class X> Alloc::Iter<X> Alloc::end() { return Iter<X>(*this, getTotal(sizeof(X)), static_cast<uint32_t>(sizeof(X))); }

} // namespace vtf

#endif // __VTF_ZDEVICEMEMORY_HPP_INCLUDED__
