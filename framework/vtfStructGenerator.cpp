#include "vtfZDeletable.hpp"
#include "vtfStructGenerator.hpp"
#include "vtfCUtils.hpp"

namespace vtf
{

template<class T>
std::vector<sg::INode::Cmp::State> singleTypeTest(
	add_cref<StructGenerator> sg,
	add_ref<std::stack<sg::INode::Cmp>> stack,
	bool asArray, T const&, sg::INodePtr str1, sg::INodePtr str2,
	add_ref<float> seed)
{
	std::vector<sg::INode::Cmp::State> results;
	const auto field1 = str1 ? str1 : sg.makeField(T());
	const auto node1 = asArray ? sg.makeArrayField(field1, 3) : field1;
	node1->loop(seed);
	const auto field2 = str2 ? str2 : sg.makeField(T());
	auto node2 = asArray ? sg.makeArrayField(field2, 3) : field2;
	sg::INode::Cmp cmp;
	cmp.lhsNode = node1;
	cmp.rhsNode = node2;
	cmp.fieldIndex = 0;
	stack.push(cmp);
	const sg::INode::Cmp::State r12 = node1->cmp(stack, true);
	const auto node3 = node1->clone();
	cmp.rhsNode = node3;
	stack.push(cmp);
	const sg::INode::Cmp::State r13 = node1->cmp(stack, true);

	return { r12, r13 };
}

bool StructGenerator::selfTest() const
{
	std::vector<sg::INode::Cmp::State> results;
	std::stack<INode::Cmp> s;
	float seed = 101;

	results = singleTypeTest(*this, s, false, int(), nullptr, nullptr, seed);
	results = singleTypeTest(*this, s, false, sg::vec<3>(), nullptr, nullptr, seed);
	results = singleTypeTest(*this, s, false, sg::mat<3, 2>(), nullptr, nullptr, seed);
	results = singleTypeTest(*this, s, true, sg::mat<3, 2>(), nullptr, nullptr, seed);
	results = singleTypeTest(*this, s, true, sg::vec<3>(), nullptr, nullptr, seed);
	results = singleTypeTest(*this, s, true, int(), nullptr, nullptr, seed);

	auto intField = makeField(int());
	auto floatField = makeField(float());
	auto s1 = generateStruct("A", { intField, floatField });
	auto s2 = generateStruct("B", { intField, floatField });
	results = singleTypeTest(*this, s, false, int(), s1, s2, seed);
	auto s3 = generateStruct("A", { floatField, intField });
	results = singleTypeTest(*this, s, false, int(), s1, s3, seed);

	return false;
}

void StructGenerator::deserializeStruct(const void* src, INodePtr structNode,
										int nesting, INode::SDCallback sdCallback,
										const INode::Control& c) const
{
	ASSERTMSG(structNode && structNode->getKind() == sg::Kind::Struct, "structNode must be structure node");
	std::size_t offset = 0;
	deserializeStructWithOffset(src, offset, structNode, nesting, sdCallback, c);
}
void StructGenerator::serializeStruct(INodePtr structNode, void* dst,
										int nesting, INode::SDCallback sdCallback,
										const INode::Control& c) const
{
	ASSERTMSG(structNode && structNode->getKind() == sg::Kind::Struct, "structNode must be structure node");
	std::size_t offset = 0;
	serializeStructWithOffset(structNode, dst, offset, nesting, sdCallback, c);
}

bool StructGenerator::composite(
	INodePtr node, add_cref<std::vector<uint8_t>> path,uint32_t depth,
	add_ref<std::ostream> pathLog, add_ref<std::ostream> valueLog)
{
	std::stack<INode::Composite> chain;
	const bool r = node->composite(path, depth, 0, 0, chain);
	std::vector<INode::Composite> list; list.reserve(chain.size());
	while (false == chain.empty()) {
		list.insert(list.begin(), chain.top());
		chain.pop();
	}
	const auto chainSize = data_count(list);
	for (uint32_t i = 0u; i < chainSize; ++i) {
		add_cref<INode::Composite> c = list[i];
		if (i) pathLog << '.';
		const bool isArray = c.parent->getKind() == sg::Kind::Array;
		const uint32_t rank = isArray ? c.item : c.nesting;
		pathLog << c.parent->genFieldName(rank, isArray, rank);
	}
	if (r) {
		add_cref<INode::Composite> c = list.back();
		c.node->printValue(valueLog, c.item);
	}
	return r;
}

bool StructGenerator::_compareTypes(
	INodePtr t1, INodePtr t2,
	add_ref<std::string> msg,
	add_ref<std::string> lhsValue, add_ref<std::string> rhsValue,
	bool compareValues) const
{
	ASSERTMSG(t1, "t1 must not be null");
	std::stack<sg::INode::Cmp > stack;
	sg::INode::Cmp cmp;
	cmp.fieldIndex = 0u;
	cmp.lhsNode = t1;
	cmp.rhsNode = t2;
	stack.push(cmp);
	const sg::INode::Cmp::State state = t1->cmp(stack, compareValues);
	bool once = true;
	if (state != sg::INode::Cmp::State::OK)
	{
		strings ss;
		ss.reserve(stack.size());
		while (false == stack.empty())
		{
			if (once) {
				lhsValue = stack.top().lhsValue;
				rhsValue = stack.top().rhsValue;
				once = false;
			}
			std::string s = stack.top().lhsNode->genFieldName(stack.top().fieldIndex, false);
			if (stack.top().index1 >= 0)
				s += '[' + std::to_string(stack.top().index1) + ']';
			if (stack.top().index2 >= 0)
				s += '[' + std::to_string(stack.top().index2) + ']';
			ss.insert(ss.begin(), std::move(s));
			stack.pop();
		}
		for (add_cref<std::string> s : ss)
		{
			if (false == s.empty())
				msg += '.' + s;
		}
		return false;
	}
	return true;
}

namespace sg
{

INode::Control::Control(const void* a, const void* b)
	: m_a(reinterpret_cast<ptrdiff_t>(a))
	, m_b(reinterpret_cast<ptrdiff_t>(b))
{
}

template<class> struct StackView;
template<class X, class C> struct StackView<std::stack<X, C>> : public std::stack<X, C>
{
	using std::stack<X, C>::c;
};

void INode::Control::test(const void* p, size_t offset, size_t bytes, bool serialize, add_cref<std::stack<INode*>> nodeStack) const
{
	if (m_b == 0) return;
	const ptrdiff_t c = reinterpret_cast<ptrdiff_t>(p);
	if (const bool ok = c >= m_a && (c + make_signed(bytes)) < m_b; false == ok)
	{
		strings nodeNames;
		add_cref<std::stack<INode*>::container_type> rc =
			static_cast<add_cref<StackView<std::stack<INode*>>>>(nodeStack).c;
		for (INode* node : rc)
		{
			nodeNames.insert(nodeNames.begin(), node->genFieldName(0, true));
		}

		int i = 0;
		std::ostringstream fieldAccess;
		for (add_cref<std::string> nodeName : nodeNames) {
			if (i) fieldAccess << '.';
			fieldAccess << nodeName;
		}

		std::ostringstream diff;
		if (c < m_a)
			diff << "less that lower-bound address with " << (m_a - c) << " bytes";
		else
			diff << "greater than upper-bound address with " << (c + make_signed(bytes) - m_b) << " bytes";

		std::cout << "Access violation, " << (serialize ? "writting" : "reading")
			<< " address (" << c << ", dword: ~" << (offset / 4) << ", bytes: " << bytes << ") "
			<< "from range (" << m_a << ", dword: ~" << (offset / 4) << "; "
			<< m_b << ", dword: ~" << (offset / 4) << ") "
			<< (serialize ? "to" : "from") << " field " << fieldAccess.str()
			<< ", " << diff.str() << std::endl;
	}
}

void INode::serializeOrDeserialize(const void* /*src*/, void* /*dst*/, size_t& /*offset*/,
									bool /*serialize*/, int& /*nesting*/,
									std::stack<INode*>&, const Control&, SDCallback) {
}

} // namespace sg
} // namespace vtf
