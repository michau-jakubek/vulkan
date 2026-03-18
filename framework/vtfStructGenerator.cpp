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

} // namespace vtf
