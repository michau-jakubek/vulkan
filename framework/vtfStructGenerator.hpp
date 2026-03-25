#ifndef __VTF_STRUCT_GENERATOR_HPP_INCLUDED__
#define __VTF_STRUCT_GENERATOR_HPP_INCLUDED__

#include <string>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <stack>
#include <vector>
#include <iostream>

namespace vtf
{
namespace sg
{
struct INode;
typedef std::shared_ptr<INode> INodePtr;
template<class> struct Node;
enum class Kind : int { None, Scalar, Vector, Matrix, Array, Struct };

#define ASSERT(x, msg) if (!(x)) throw std::runtime_error(msg)

struct INode
{
    friend struct Node<INodePtr>;
    const std::string typeName;
    INodePtr next;
    virtual INodePtr getChildren(bool raise = true) const {
        if (raise) {
            const std::string msg = "No overriden " + typeName + "::getChildren() found";
            throw std::runtime_error(msg);
        }
        return nullptr;
    }
    virtual INodePtr getLastChild(bool raise = true) const {
        return getChildren(raise);
    }
    virtual INodePtr getElementType() const {
        return nullptr;
    }
    virtual Kind getKind() const {
        return Kind::None;
    }
    virtual INodePtr clone() const {
        return nullptr;
    }
    struct Composite {
        INode* parent;
        INodePtr node;
        uint32_t item;
        uint32_t nesting;
    };
    virtual bool composite(
        add_cref<std::vector<uint8_t>> /*path*/,
        uint32_t /*depth*/, uint32_t /*item*/, uint32_t /*nesting*/,
        add_ref<std::stack<Composite>> /*chain*/) const {
        return false;
    }
    struct Cmp {
        enum State { Undefined, OK, TypeMismatch, ValueMismatch };
        INodePtr lhsNode, rhsNode;
        std::string lhsValue, rhsValue;
        int index1 = (-1);
        int index2 = (-1);
        uint32_t fieldIndex = 0u;
    };
    virtual Cmp::State cmp(add_ref<std::stack<Cmp>>, bool /*compareValue*/) const {
        return Cmp::State::Undefined;
    }
    virtual void loop(float& /*seedValue*/) { }
    virtual std::string genFieldName(uint32_t index, bool /*appendRank*/ = false, uint32_t /*rank*/ = 0) const {
        return typeName + '_' + std::to_string(index);
    }
    virtual void printValue(std::ostream&, uint32_t) const {}
    virtual std::size_t getBaseAlignment() const {
        ASSERT(false, "getBaseAlignment() not defined");
        return 0u;
    }
    virtual std::size_t getLogicalSize() const { return 0; }
    enum class SDAction : uint32_t {
        None, Serialize, Deserialize,
        SerializeStruct, DeserializeStruct,
        SerializeArray,  DeserializeArray,
        SerializeVector, DeserializeVector,
        SerializeMatrix, DeserializeMatrix,
        UpdatePadBefore, UpdatePadAfter };
    struct SDParams
    {
        std::size_t alignment;
        std::size_t size;
        std::size_t offset;
        std::size_t pad;
        std::size_t stride;
        uint32_t    fieldIndex;
        int         nesting;
        SDAction    action;
        SDAction    whenAction;
        std::string fieldType;
        bool        inArray;
    };
    using SDCallback = std::function<void(const SDParams&)>;
    struct Control {
        const ptrdiff_t m_a;
        const ptrdiff_t m_b;
        Control(const void* a, const void* b);
        void test(const void* p, size_t offset, size_t bytes, bool serialize, add_cref<std::stack<INode*>>) const;
    };
    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& offset,
                                        bool serialize, int& nesting,
                                        std::stack<INode*>& nodeStack, const Control& , SDCallback sdCallback);
    static void putIndent(std::ostream& str, uint32_t indent) {
        str << std::string((indent * 4u), ' ');
    }
    virtual uint32_t getVisitCount() const {
        ASSERT(false, "getVisitCount() not implemented");
        return 0;
    }
    virtual void genLoops(
        std::ostream&,      // str
        const std::string&, // currentPath
        const std::string&, // rhsCode
        uint32_t,           // fieldIndex
        uint32_t,           // indent
        uint32_t&           // iteratorSeed
    ) const {}
    virtual ~INode() = default;
protected:
	INode(const std::string& typeName_, INodePtr) : typeName(typeName_) {}
};

template<std::size_t K, class T> struct vec;
template<std::size_t Cols, std::size_t Rows, class T, bool ColumnMajor> struct mat;
template<class X, std::size_t Elems, bool IsRuntime> struct Array;

template<class> const char* type_to_string = "";
template<> inline constexpr const char* type_to_string<int32_t> = "int";
template<> inline constexpr const char* type_to_string<uint32_t> = "uint";
template<> inline constexpr const char* type_to_string<float> = "float";
template<class X, std::size_t Elems, bool IsRuntime>
inline constexpr const char* type_to_string<Array<X, Elems, IsRuntime>> = type_to_string<X>;

template<class X> struct Node : public INode
{
	X value{};
	Node(const std::string& /*typeName*/, INodePtr) : INode(type_to_string<X>, nullptr) {}
	virtual void printValue(std::ostream& str, uint32_t) const override
	{
		str << value;
	}
    virtual Kind getKind() const override { return Kind::Scalar; }
    virtual INodePtr clone() const override {
		auto c = std::make_shared<Node>(type_to_string<X>, nullptr);
        c->value = value;
        return c;
	}
    virtual bool composite(
        add_cref<std::vector<uint8_t>> path,
        uint32_t depth, uint32_t item, uint32_t nesting,
        add_ref<std::stack<Composite>> chain) const override
    {
        if (depth <= path.size()) {
            Composite c;
            c.parent = const_cast<Node*>(this);
            c.node = clone();
            c.item = item;
            c.nesting = nesting;
            chain.push(c);
            return true;
        }
        return false;
    }
    virtual void loop(float& seed) override {
		value = X(seed);
        seed += 1.0f;
	}
    virtual Cmp::State cmp(add_ref<std::stack<Cmp>> nodeStack, bool compareValue) const override {
        auto other = std::dynamic_pointer_cast<Node>(nodeStack.top().rhsNode);
        if (nullptr == other)
            return Cmp::State::TypeMismatch;
        if (compareValue && value != other->value) {
            nodeStack.top().lhsValue = std::to_string(value);
            nodeStack.top().rhsValue = std::to_string(other->value);
            return Cmp::State::ValueMismatch;
        }
        return Cmp::State::OK;
    }

    virtual std::size_t getBaseAlignment() const override {
        return sizeof(X);
    }
    virtual std::size_t getLogicalSize() const override {
        return sizeof(X);
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& offset,
                                        bool serialize, int& /*nesting*/,
                                        std::stack<INode*>& nodeStack, const Control& c, SDCallback) override
    {
        nodeStack.push(this);
        if (serialize)
        {
            X* p = reinterpret_cast<X*>(reinterpret_cast<std::byte*>(dst) + offset);
            c.test(p, offset, getLogicalSize(), serialize, nodeStack);
            *p = value;
        }
        else
        {
            const X* p = reinterpret_cast<const X*>(reinterpret_cast<const std::byte*>(src) + offset);
            c.test(p, offset, getLogicalSize(), serialize, nodeStack);
            value = *p;
        }
        nodeStack.pop();
    }
    virtual uint32_t getVisitCount() const override {
        return 1u;
    };

    virtual void genLoops(
        std::ostream& str,
        const std::string& currentPath,
        const std::string& rhsCode,
        uint32_t, // fieldIndex
        uint32_t indent,
        uint32_t& iteratorSeed) const override
    {
        iteratorSeed = iteratorSeed + 1u;
        putIndent(str, indent);
        str << currentPath << " = " << typeName << '(' << rhsCode << ");\n";
    }
};

template<> struct Node<INodePtr> : public INode
{
    INodePtr children;
    Node(const std::string&, INodePtr ch)
        : INode(ch->typeName, ch)
        , children(cloneChildren(ch))
    {
    }
    virtual INodePtr getChildren(bool) const override {
        return children;
    }
    virtual void printValue(std::ostream& str, uint32_t level) const override
    {
        uint32_t childIndex = 0;
        str << genFieldName(level, true) << " {\n";
        for (INodePtr p = children->next; p; p = p->next)
        {
            if (p->getKind() == Kind::Struct)
            {
                str << "    " << p->genFieldName(childIndex) << " = {...}";
            }
            else
            {
                str << "    " << p->genFieldName(childIndex) << " = ";
                p->printValue(str, level + 1);

            }
            str << '\n';
            childIndex += 1u;
        }
        str << '}';
    }
    static INodePtr createEmpty(const std::string& typeName)
    {
        INodePtr children(new INode(typeName, nullptr));
        return std::make_shared<Node>(typeName, children);
    }
    INodePtr cloneChildren(INodePtr ch) const {
        INodePtr list(new INode(ch->typeName, nullptr));
        INodePtr* nextField = &list->next;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            *nextField = p->clone();
            nextField = &nextField->get()->next;
        }
        return list;
    }
    virtual INodePtr getLastChild(bool) const override {
        INodePtr ch = children;
        while (ch->next)
            ch = ch->next;
        return ch;
    }
    virtual Kind getKind() const override { return Kind::Struct; }
    virtual INodePtr clone() const override {
        return std::make_shared<Node>(typeName, children);
    }
    virtual bool composite(
        add_cref<std::vector<uint8_t>> path,
        uint32_t depth, uint32_t item, uint32_t nesting,
        add_ref<std::stack<Composite>> chain) const override
    {
        if (depth <= path.size() && item < depth) {
            uint32_t childIndex = 0u;
            for (INodePtr p = children->next; p; p = p->next) {
                if (path[item] == childIndex++) {
                    Composite c;
                    c.parent = const_cast<Node*>(this);
                    c.node = p;
                    c.item = path[item];
                    c.nesting = nesting;
                    chain.push(c);
                    return ((item + 1u) == depth) ? true : p->composite(path, depth, (item + 1), (nesting + 1), chain);
                }
            }
        }
        return false;
    }
    virtual void loop(float& seed) override {
        for (INodePtr p = children->next; p; p = p->next)
            p->loop(seed);
    }
    virtual Cmp::State cmp(add_ref<std::stack<Cmp>> nodeStack, bool compareValue) const override
    {
        uint32_t fieldIndex = 0u;
        INodePtr myChild = children->next;
        INodePtr otherChild = nodeStack.top().rhsNode->getChildren(false)
            ? nodeStack.top().rhsNode->getChildren(false)->next : nullptr;
        while (myChild && otherChild)
        {
            Cmp c;
            c.lhsNode = myChild;
            c.rhsNode = otherChild;
            c.fieldIndex = fieldIndex;
            nodeStack.push(c);
            const Cmp::State cmpResult = myChild->cmp(nodeStack, compareValue);
            if (cmpResult == Cmp::State::OK)
            {
                nodeStack.pop();
                otherChild = otherChild->next;
                myChild = myChild->next;
                fieldIndex = fieldIndex + 1u;
            }
            else
            {
                return cmpResult;
            }
        }
        if ((nullptr == myChild) ^ (nullptr == otherChild))
        {
            return Cmp::State::TypeMismatch;
        }
        return Cmp::State::OK;
    }
    virtual std::size_t getBaseAlignment() const override {
        size_t maxAb = 0u;
        for (INodePtr p = children->next; p; p = p->next) {
            maxAb = std::max(maxAb, p->getBaseAlignment());
        }
        return maxAb;
    }
    static std::size_t updateOffsetWithPadding(const INode* field, std::size_t baseAlignment, std::size_t& offset)
    {
        const std::size_t Ab = field ? field->getBaseAlignment() : baseAlignment;
        const std::size_t alignedOffset = ((offset + Ab - 1u) / Ab) * Ab;
        const std::size_t padding = alignedOffset - offset;
        offset += padding;
        return padding;
    }
    virtual std::size_t getLogicalSize() const override
    {
        const std::size_t structAb = getBaseAlignment();

        std::size_t offset = 0;

        for (INodePtr p = children->next; p; p = p->next)
	{
            const std::size_t fieldAb = p->getBaseAlignment();
            const std::size_t fieldSize = p->getLogicalSize();
            updateOffsetWithPadding(nullptr, fieldAb, offset);
            offset += fieldSize;
        }

        updateOffsetWithPadding(nullptr, structAb, offset);

        return offset;
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& offset,
        bool serialize, int& nesting,
        std::stack<INode*>& nodeStack, const Control& c, SDCallback sdCallback) override
    {
        nesting = nesting + 1;

        const std::string& structName = typeName; static_cast<void>(structName);
        std::string fieldName = genFieldName(uint32_t(nesting), true);
        const std::size_t thisStructAb = getBaseAlignment();
        const std::size_t thisStructSize = sdCallback ? getLogicalSize() : 1u;

        const std::size_t initialPadding = updateOffsetWithPadding(nullptr, thisStructAb, offset);
        if (initialPadding && sdCallback)
        {
            SDParams sdp{};
            sdp.action = SDAction::UpdatePadBefore;
            sdp.whenAction = serialize ? SDAction::Serialize : SDAction::Deserialize;
            sdp.alignment = thisStructAb;
            sdp.fieldIndex = 9999;
            sdp.fieldType = typeName;
            sdp.nesting = nesting;
            sdp.offset = (offset - initialPadding);
            sdp.pad = initialPadding;
            sdp.size = thisStructSize;
            sdp.inArray = false;
            sdCallback(sdp);
        }

        uint32_t childIndex = 0u;

        for (INodePtr p = children->next; p; p = p->next, ++childIndex) {
            fieldName = p->genFieldName(uint32_t(nesting), true);
            const std::size_t fieldAlignment = p->getBaseAlignment();
            const std::size_t fieldLogicalSize = p->getLogicalSize();

            const std::size_t pad = updateOffsetWithPadding(nullptr, fieldAlignment, offset);
            if (pad && sdCallback)
            {
                SDParams sdp{};
                sdp.action = SDAction::UpdatePadBefore;
                sdp.whenAction = serialize ? SDAction::SerializeStruct : SDAction::DeserializeStruct;
                sdp.alignment = fieldAlignment;
                sdp.fieldIndex = childIndex;
                sdp.fieldType = typeName + "::" + fieldName;
                sdp.nesting = nesting;
                sdp.offset = (offset - pad);
                sdp.pad = pad;
                sdp.size = fieldLogicalSize;
                sdp.inArray = false;
                sdCallback(sdp);
            }
            if (sdCallback)
            {
                SDParams sdp{};
                sdp.action = serialize ? SDAction::SerializeStruct : SDAction::DeserializeStruct;
                sdp.whenAction = sdp.action;
                sdp.alignment = fieldAlignment;
                sdp.fieldIndex = childIndex;
                sdp.fieldType = typeName + "::" + fieldName;
                sdp.nesting = nesting;
                sdp.offset = offset;
                sdp.pad = 0;
                sdp.size = fieldLogicalSize;
                sdp.inArray = false;
                sdCallback(sdp);
            }

            p->serializeOrDeserialize(src, dst, offset, serialize, nesting, nodeStack, c, sdCallback);

            if (p->getKind() != Kind::Struct)
            {
                offset = offset + fieldLogicalSize;
            }
        }

        const std::size_t finalPadding = updateOffsetWithPadding(nullptr, thisStructAb, offset);
        if (finalPadding && sdCallback)
        {
            SDParams sdp{};
            sdp.action = SDAction::UpdatePadAfter;
            sdp.whenAction = serialize ? SDAction::SerializeStruct : SDAction::DeserializeStruct;
            sdp.alignment = thisStructAb;
            sdp.fieldIndex = 9999;
            sdp.fieldType = typeName;
            sdp.nesting = nesting;
            sdp.offset = (offset - finalPadding);
            sdp.pad = finalPadding;
            sdp.size = thisStructSize;
            sdp.inArray = false;
            sdCallback(sdp);
        }

        nesting = nesting - 1;
    }

    virtual uint32_t getVisitCount() const override {
        uint32_t visitCount = 0u;
        for (INodePtr p = children->next; p; p = p->next)
            visitCount += p->getVisitCount();
        return visitCount;
    }
    virtual void genLoops(
        std::ostream& str,
        const std::string& currentPath,
        const std::string& rhsCode,
        uint32_t fieldIndex,
        uint32_t indent,
        uint32_t& iteratorSeed) const override
    {
        fieldIndex = 0u;
        for (INodePtr p = children->next; p; p = p->next)
        {
            p->genLoops(str, (currentPath + '.' + p->genFieldName(fieldIndex)),
                rhsCode, fieldIndex, indent, iteratorSeed);
            fieldIndex = fieldIndex + 1u;
        }
    }
};

template<std::size_t Elems, bool IsRuntime>
struct Node<Array<INodePtr, Elems, IsRuntime>>
    : public INode, public std::enable_shared_from_this<Node<Array<INodePtr, Elems, IsRuntime>>>
{
    static_assert(Elems != 0, "Array size must be a positive integer");
    std::vector<INodePtr> value;
    const std::size_t dynamicElems;
    const bool dynamicIsRuntime;
    const std::string privateName;
    Node(const std::string& /*typeName*/, INodePtr structure) 
        : INode(structure->typeName, nullptr)
        , value(Elems)
        , dynamicElems(Elems)
        , dynamicIsRuntime(IsRuntime)
        , privateName()
    {
        for (uint32_t i = 0u; i < Elems; ++i)
            value[i] = structure->clone();
    }
    Node(INodePtr elemType, std::size_t elemCount, bool isRuntimeArray = false)
        : INode(elemType->typeName, nullptr)
        , value(elemCount)
        , dynamicElems(elemCount)
        , dynamicIsRuntime(isRuntimeArray)
        , privateName()
    {
        ASSERTMSG(elemCount, "Array size must be a positive integer");
        for (uint32_t i = 0u; i < elemCount; ++i)
            value[i] = elemType->clone();
    }
    std::string checkElems(add_cref<std::vector<INodePtr>> elems) {
        ASSERTMSG(elems.size(), "Array size must be a positive integer");
        if (elems.size() > 1u) {
            Cmp c;
            std::stack<Cmp> st;
            st.push(c);
            for (uint32_t i = 1u; i < elems.size(); ++i) {
                st.top().lhsNode = elems[0];
                st.top().rhsNode = elems[i];
                ASSERTMSG(elems[0]->cmp(st, false) == Cmp::State::OK,
                    "Array elements must be the same type");
            }
        }
        return elems[0]->typeName;
    }
    Node(add_cref<std::vector<INodePtr>> elems, bool isRuntimeArray = false,
            add_cref<std::string> arrayName = std::string())
        : INode(checkElems(elems), nullptr)
        , value(elems.size())
        , dynamicElems(elems.size())
        , dynamicIsRuntime(isRuntimeArray)
        , privateName(arrayName)
    {
        for (uint32_t i = 0u; i < dynamicElems; ++i)
            value[i] = elems[i]->clone();
    }
    virtual Kind getKind() const override { return Kind::Array; }
    virtual INodePtr clone() const override {
        auto c = std::make_shared<Node<Array<INodePtr, 1, false>>>(value[0], dynamicElems, dynamicIsRuntime);
        for (uint32_t i = 0u; i < dynamicElems; ++i)
            c->value[i] = value[i]->clone();
        return c;
    }
    virtual bool composite(
        add_cref<std::vector<uint8_t>> path,
        uint32_t depth, uint32_t item, uint32_t nesting,
        add_ref<std::stack<Composite>> chain) const override
    {
        if (depth <= path.size() && item < depth && path[item] < dynamicElems) {
            Composite c;
            c.parent    = const_cast<Node*>(this);
            c.node      = value[path[item]];
            c.item      = path[item];
            c.nesting   = nesting;
            chain.push(c);
            return ((item + 1u) == depth) ? true : c.node->composite(path, depth, (item + 1), nesting, chain);
        }
        return false;
    }
    virtual INodePtr getElementType() const override {
        return value[0];
    }
    virtual std::string genFieldName(uint32_t index, bool appendRank, uint32_t /*rank*/ = 0) const override {
        const std::string fieldName = privateName.empty() ? INode::genFieldName(index) : privateName;
        if (appendRank) {
            return fieldName + '[' + (dynamicIsRuntime
                                        ? ("/* " + std::to_string(dynamicElems) + " */")
                                        : std::to_string(dynamicElems)) + ']';
        }
        return fieldName;
    }
    virtual void printValue(std::ostream& str, uint32_t level) const override {
        if (value[0]->getKind() == Kind::Scalar)
            for (std::size_t i = 0; i < dynamicElems; ++i)
            {
                if (i) str << ' ';
                str << '[' << i << "]:";
                value[i]->printValue(str, level + 1);
            }
        else str << "{ " << dynamicElems << " * " << value[0]->typeName << " }";
    }
    virtual void loop(float& seed) override {
        for (std::size_t i = 0; i < dynamicElems; ++i) {
            value[i]->loop(seed);
        }
    }
    virtual Cmp::State cmp(add_ref<std::stack<Cmp>> nodeStack, bool compareValue) const override {
        auto otherArray = std::dynamic_pointer_cast<Node>(nodeStack.top().rhsNode);
        if (nullptr == otherArray)
            return Cmp::State::TypeMismatch;
        if (dynamicElems != otherArray->dynamicElems)
            return Cmp::State::TypeMismatch;
        if (compareValue)
            for (uint32_t i = 0; i < dynamicElems; ++i) {
                Cmp cmp;
                cmp.fieldIndex = nodeStack.top().fieldIndex;
                cmp.index1 = int(i);
                cmp.lhsNode = value[i];
                cmp.rhsNode = otherArray->value[i];
                nodeStack.push(cmp);
                const auto state = value[i]->cmp(nodeStack, compareValue);
                if (state != Cmp::State::OK)
                    return state;
                nodeStack.pop();
            }
        return Cmp::State::OK;
    }
    virtual std::size_t getBaseAlignment() const override {
        return value[0]->getBaseAlignment();
    }
    virtual std::size_t getLogicalSize() const override {
        return dynamicElems * getStride();
    }
    std::size_t getStride() const {
        std::size_t offset = value[0]->getLogicalSize();
        Node<INodePtr>::updateOffsetWithPadding(nullptr, getBaseAlignment(), offset);
        return offset;
    }
    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& offset,
                                        bool serialize, int& nesting,
                                        std::stack<INode*>& nodeStack, const Control& c, SDCallback sdCallback) override
    {
        const std::size_t stride = getStride();

        std::size_t rwOffset = offset;
        for (std::size_t i = 0; i < dynamicElems; ++i)
        {
            std::size_t elementOffset = 0u;

            if (sdCallback)
            {
                SDParams sdp{};
                sdp.action      = serialize ? SDAction::SerializeArray : SDAction::DeserializeArray;
                sdp.whenAction  = sdp.action;
                sdp.alignment   = stride;
                sdp.fieldIndex  = uint32_t(i);
                sdp.fieldType   = value[0]->typeName + '[' + std::to_string(i) + ']';
                sdp.nesting     = nesting;
                sdp.offset      = rwOffset;
                sdp.size        = value[0]->getLogicalSize();
                sdp.pad         = 0;
                sdp.stride      = stride;
                sdp.inArray     = false;
                sdCallback(sdp);
            }

            nodeStack.push(value[i].get());
            if (serialize) {
                std::byte* elementAddr = reinterpret_cast<std::byte*>(dst) + rwOffset;
                c.test(elementAddr, rwOffset, stride, serialize, nodeStack);
                value[i]->serializeOrDeserialize(nullptr, elementAddr, elementOffset, true, nesting,
                                                    nodeStack, c, sdCallback);
            } 
            else {
                const std::byte* elementAddr = reinterpret_cast<const std::byte*>(src) + rwOffset;
                c.test(elementAddr, rwOffset, stride, serialize, nodeStack);
                value[i]->serializeOrDeserialize(elementAddr, nullptr, elementOffset, false, nesting,
                                                    nodeStack, c, sdCallback);
            }
            nodeStack.pop();
            rwOffset = rwOffset + stride;
        }
    }
    virtual uint32_t getVisitCount() const override {
        return uint32_t(value[0]->getVisitCount() * dynamicElems);
    }
    virtual void genLoops(
        std::ostream& str,
        const std::string& currentPath,
        const std::string& rhsCode,
        uint32_t fieldIndex,
        uint32_t indent,
        uint32_t& iteratorSeed) const override
    {
        const std::string it = genFieldName(fieldIndex, false) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << it << " = 0; " << it << " < " << dynamicElems << "; ++" << it << ") {\n";
        value[0]->genLoops(str, (currentPath + '[' + it + ']'),
                          rhsCode, 0, (indent + 1), iteratorSeed);
        putIndent(str, indent);
        str << "}\n";
    }
};

template<std::size_t Elems, bool IsRuntime = false>
Array<INodePtr, Elems, IsRuntime> makeArray(INodePtr structType)
{
    return Array<INodePtr, Elems, IsRuntime>(structType);
}

template<std::size_t Cols, std::size_t Rows, class T = float, bool ColumnMajor = true> struct mat {};
template<std::size_t Cols, size_t Rows, class T, bool ColumnMajor>
struct Node<mat<Cols, Rows, T, ColumnMajor>> : public INode
{
    static_assert(Cols == 2 || Cols == 3 || Cols == 4 || Rows == 2 || Rows == 3 || Rows == 4, "???");
    T value[Cols * Rows]{};

    virtual Kind getKind() const override { return Kind::Matrix; }

    virtual INodePtr clone() const override {
        auto c = std::make_shared<Node>(genTypeName(), nullptr);
        for (uint32_t R = 0u; R < Rows; ++R)
            for (uint32_t C = 0u; C < Cols; ++C)
                c->value[R * Cols + C] = value[R * Cols + C];
        return c;
    }

    virtual bool composite(
        add_cref<std::vector<uint8_t>> path,
        uint32_t depth, uint32_t item, uint32_t nesting,
        add_ref<std::stack<Composite>> chain) const override
    {
        if (depth <= path.size() && item < depth && path[item] < (Cols * Rows)) {
            Composite c;
            c.parent = const_cast<Node*>(this);
            c.node = std::make_shared<Node<T>>(type_to_string<T>, nullptr);
            c.item = path[item];
            c.nesting = nesting;
            std::dynamic_pointer_cast<Node<T>>(c.node)->value = value[path[item]];
            chain.push(c);
            return ((item + 1u) == depth) ? true : c.node->composite(path, depth, (item + 1), nesting, chain);
        }
        return false;
    }

    std::string genTypeName() const {
        std::ostringstream os;
        os << (std::is_unsigned_v<T> ? "u" : "")
           << (std::is_integral_v<T> ? "i" : "")
           << "mat";
        if constexpr (Cols == Rows)
            os << Cols;
        else
            os << Cols << 'x' << Rows;
        return os.str();
    }

    Node(const std::string&, INodePtr) : INode(genTypeName(), nullptr) {}

    virtual void printValue(std::ostream& str, uint32_t) const override {
        for (std::size_t C = 0u; C < Cols; ++C) {
            if (C) str << ' ';
            for (std::size_t R = 0u; R < Rows; ++R) {
                if (R) str << ' ';
                str << '[' << C << ',' << R << "]:" << value[C * Rows + R];
            }
        }
    }

    virtual INodePtr getElementType() const override {
        return std::make_shared<Node<T>>(std::string(), nullptr);
    }

    virtual void loop(float& seed) override {
        for (std::size_t C = 0u; C < Cols; ++C)
            for (std::size_t R = 0u; R < Rows; ++R) {
                value[C * Rows + R] = T(seed);
                seed += 1.f;
            }
    }

    virtual Cmp::State cmp(add_ref<std::stack<Cmp>> nodeStack, bool compareValue) const override {
        auto otherMat = std::dynamic_pointer_cast<Node>(nodeStack.top().rhsNode);
        if (nullptr == otherMat)
            return Cmp::State::TypeMismatch;
        if (compareValue)
            for (std::size_t C = 0u; C < Cols; ++C)
                for (std::size_t R = 0u; R < Rows; ++R) {
                    const std::size_t I = C * Rows + R;
                    if (value[I] != otherMat->value[I])
                    {
                        nodeStack.top().index1 = int(R);
                        nodeStack.top().index2 = int(C);
                        nodeStack.top().lhsValue = std::to_string(value[I]);
                        nodeStack.top().rhsValue = std::to_string(otherMat->value[I]);
                        return Cmp::State::ValueMismatch;
                    }
                }
        return Cmp::State::OK;
    }

    std::size_t getColumnStride() const {
        constexpr std::size_t vecLength = ColumnMajor ? Rows : Cols;
        constexpr std::size_t normLength = (vecLength == 3 ? 4 : vecLength);
        return normLength * sizeof(T);
    }

    std::size_t getMatrixSize() const {
        constexpr std::size_t columnCount = ColumnMajor ? Cols : Rows;
        return columnCount * getColumnStride();
    }

    virtual std::size_t getBaseAlignment() const override {
        return getColumnStride();
    }

    virtual std::size_t getLogicalSize() const override {
        return getMatrixSize();
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& xoffset,
                                        bool serialize, int& nesting,
                                        std::stack<INode*>& nodeStack, const Control& c, SDCallback sdCallback) override
    {
        const std::size_t stride = getColumnStride();
        const std::size_t size = getMatrixSize();

        nodeStack.push(this);
        for (std::size_t i = 0; i < Cols; ++i) {
            std::size_t columnStart = xoffset + (i * stride);
            for (std::size_t j = 0; j < Rows; ++j) {

                if (sdCallback)
                {
                    SDParams sdp{};
                    sdp.action = serialize ? SDAction::SerializeMatrix : SDAction::DeserializeMatrix;
                    sdp.whenAction  = sdp.action;
                    sdp.alignment   = sizeof(T);
                    sdp.fieldIndex  = uint32_t(i);
                    sdp.fieldType   = typeName + '[' + std::to_string(i) + "][" + std::to_string(j) + ']';
                    sdp.nesting     = nesting;
                    sdp.offset      = columnStart + j * sizeof(T);
                    sdp.size        = sizeof(T);
                    sdp.pad         = 0;
                    sdp.stride      = stride;
                    sdp.inArray     = false;
                    sdCallback(sdp);
                }

                if (serialize)
                {
                    T* p = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(dst) + columnStart + (j * sizeof(T)));
                    c.test(p, xoffset, size, serialize, nodeStack);
                    *p = value[i * Rows + j];
                }
                else
                {
                    const T* p = reinterpret_cast<const T*>(
                        reinterpret_cast<const std::byte*>(src) + columnStart + (j * sizeof(T)));
                    c.test(p, xoffset, size, serialize, nodeStack);
                    value[i * Rows + j] = *p;
                }
            }
        }
        nodeStack.pop();
    }

    virtual uint32_t getVisitCount() const override {
        return uint32_t(Cols * Rows);
    }

    virtual void genLoops(
        std::ostream& str,
        const std::string& currentPath,
        const std::string& rhsCode,
        uint32_t fieldIndex,
        uint32_t indent,
        uint32_t& iteratorSeed) const override
    {
        const std::string ic = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        const std::string ir = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << ic << " = 0; " << ic << " < " << Cols << "; ++" << ic << ") {\n";
        putIndent(str, indent + 1u);
        str << "for (uint " << ir << " = 0; " << ir << " < " << Rows << "; ++" << ir << ") {\n";
        putIndent(str, indent + 2u);
        const std::string path = currentPath + '[' + ic + "][" + ir + ']';
        str << path << " = " << getElementType()->typeName << '(' << rhsCode << ");\n";
        putIndent(str, indent + 1u);
        str << "}\n";
        putIndent(str, indent);
        str << "}\n";
    }
};

template<std::size_t K, class T = float> struct vec {};
template<std::size_t K, class T>
struct Node<vec<K, T>> : public INode
{
    static_assert(false == std::is_same_v<T, INodePtr>, "???");
    T value[K]{};
    using ValueType = T [K];

    virtual Kind getKind() const override { return Kind::Vector; }

    virtual INodePtr clone() const override {
        auto c = std::make_shared<Node>(genTypeName(), nullptr);
        for (uint32_t i = 0u; i < K; ++i)
            c->value[i] = value[i];
        return c;
    }

    virtual bool composite(
        add_cref<std::vector<uint8_t>> path,
        uint32_t depth, uint32_t item, uint32_t nesting,
        add_ref<std::stack<Composite>> chain) const override
    {
        if (depth <= path.size() && item < depth && path[item] < K) {
            Composite c;
            c.parent = const_cast<Node*>(this);
            c.node = std::make_shared<Node<T>>(type_to_string<T>, nullptr);
            c.item = path[item];
            c.nesting = nesting;
            std::static_pointer_cast<Node<T>>(c.node)->value = value[path[item]];
            chain.push(c);
            return ((item + 1u) == depth) ? true : c.node->composite(path, depth, (item + 1), nesting, chain);
        }
        return false;
    }

    std::string genTypeName() const {
        std::ostringstream os;
        os << (std::is_unsigned_v<T> ? "u" : "")
           << (std::is_integral_v<T> ? "i" : "")
           << "vec" << K;
        return os.str();
    }

    Node(const std::string&, INodePtr) : INode(genTypeName(), nullptr) {}

    virtual void printValue(std::ostream& str, uint32_t) const override {
        for (std::size_t X = 0u; X < K; ++X) {
            if (X) str << ' ';
            str << X << ':' << value[X];
        }
    }

    virtual INodePtr getElementType() const override {
        return std::make_shared<Node<T>>(std::string(), nullptr);
    }

    virtual void loop(float& seed) override {
        for (std::size_t X = 0u; X < K; ++X) {
            value[X] = T(seed);
            seed += 1.f;
        }
    }

    virtual Cmp::State cmp(add_ref<std::stack<Cmp>> nodeStack, bool compareValue) const override {
        auto otherVec = std::dynamic_pointer_cast<Node>(nodeStack.top().rhsNode);
        if (nullptr == otherVec)
            return Cmp::State::TypeMismatch;
        if (compareValue)
            for (uint32_t i = 0u; i < K; ++i)
                if (value[i] != otherVec->value[i]) {
                    nodeStack.top().index1 = int(i);
                    nodeStack.top().lhsValue = std::to_string(value[i]);
                    nodeStack.top().rhsValue = std::to_string(otherVec->value[i]);
                    return Cmp::State::ValueMismatch;
                }
        return Cmp::State::OK;
    }

    virtual std::size_t getBaseAlignment() const override {
        const std::size_t elemSize = getElementType()->getBaseAlignment();
        return (K == 3u) ? (4u * elemSize) : (K * elemSize);
    }

    virtual std::size_t getLogicalSize() const override {
        const std::size_t elemSize = getElementType()->getLogicalSize();
        return K * elemSize;
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, size_t& offset,
                                        bool serialize, int& /*nesting*/,
                                        std::stack<INode*>& nodeStack, const Control& c, SDCallback) override
    {
        nodeStack.push(this);
        std::size_t elemOffset = offset;
        const std::size_t size = getLogicalSize();

        for (std::size_t i = 0u; i < K; ++i)
        {
            if (serialize)
            {
                T* p = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(dst) + elemOffset);
                c.test(p, offset, size, serialize, nodeStack);
                *p = value[i];
            }
            else
            {
                const T* p = reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(src) + elemOffset);
                c.test(p, offset, size, serialize, nodeStack);
                value[i] = *p;
            }
            elemOffset = elemOffset + sizeof(T);
        }
        nodeStack.pop();
    }

    virtual uint32_t getVisitCount() const override {
        return uint32_t(K);
    }

    virtual void genLoops(
        std::ostream& str,
        const std::string& currentPath,
        const std::string& rhsCode,
        uint32_t fieldIndex,
        uint32_t indent,
        uint32_t& iteratorSeed) const override
    {
        const std::string it = genFieldName(fieldIndex) + '_' + std::to_string(iteratorSeed++);
        putIndent(str, indent);
        str << "for (uint " << it << " = 0; " << it << " < " << K << "; ++" << it << ") {\n";
        putIndent(str, indent + 1u);
        str << currentPath << '[' << it << ']' << " = " << getElementType()->typeName << '(' << rhsCode << ");\n";
        putIndent(str, indent);
        str << "}\n";
    }
};

} //namespace sg

struct StructGenerator
{
    using INode = sg::INode;
    using INodePtr = sg::INodePtr;
    template<typename X> using Node = sg::Node<X>;

    INodePtr generateStruct(const std::string& typeName, const std::vector<INodePtr> fields) const
    {
        INodePtr root = Node<INodePtr>::createEmpty(typeName);
        INodePtr* pNext = &root->getChildren()->next;
        for (auto field : fields)
        {
            *pNext = field->clone();
            pNext = &pNext->get()->next;
        }
        return root;
    }

    // example: makeField(int()), makeField(vec<2,int>()),
    template<class FieldType> static INodePtr makeField(const FieldType&)
    {
        return std::make_shared<Node<FieldType>>(std::string(), nullptr);
    }

    static INodePtr makeArrayField(INodePtr elemType, std::size_t elemCount, bool isRuntime = false)
    {
        return std::make_shared<Node<sg::Array<INodePtr, 1, false>>>(elemType, elemCount, isRuntime);
    }

    static INodePtr makeArrayField(const std::vector<INodePtr>& elems, bool isRuntime = false,
                            const std::string& arrayName = std::string()) 
    {
        return std::make_shared<Node<sg::Array<INodePtr, 1, false>>>(elems, isRuntime, arrayName);
    }

    static bool structureAppendField(INodePtr rootStruct, INodePtr field)
    {
        INodePtr lastField = rootStruct->getLastChild(false);
        if (lastField)
        {
            lastField->next = field->clone();
            return true;
        }
        return false;
    }

    bool compareTypes(INodePtr t1, INodePtr t2, add_ref<std::string> msg,
                        add_ref<std::string> lhsValue, add_ref<std::string> rhsValue, bool compareValues = true) const
    {
        return _compareTypes(t1, t2, msg, lhsValue, rhsValue, compareValues);
    }

    void printStruct(INodePtr structNode, std::ostream& str, bool declaration = true) const
    {
        uint32_t fieldNum = 0u;
        INodePtr ch = structNode->getChildren();

        std::string::size_type longest = 0u;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            longest = std::max(longest, p->typeName.length());
        }

        if (declaration)
        {
            str << "struct " << structNode->typeName << ' ';
        }
        str << '{' << std::endl;
        for (INodePtr p = ch->next; p; p = p->next)
        {
            str << "    " << p->typeName
                << std::string((longest - p->typeName.length() + 1), ' ')
                << p->genFieldName(fieldNum++, true) << ";\n";
        }
        str << '}';
        if (declaration)
        {
            str << ";\n";
        }
    }

    std::vector<INodePtr> getStructList(INodePtr rootStruct) const
    {
        std::vector<INodePtr> list{ rootStruct };
        _getStructList(rootStruct, list);
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            for (auto j = std::next(i); j != list.end();)
            {
                if (i->get()->typeName == j->get()->typeName)
                {
                    j = list.erase(j);
                }
                else
                {
                    ++j;
                }
            }
        }
        return list;
    }

    void generateLoops(INodePtr rootStruct, const std::string& rootStructName, std::ostream& str, uint32_t indent, const std::string& rhsCode) const
    {
        uint32_t iteratorSeed = 0;
        rootStruct->genLoops(str, rootStructName, rhsCode, 0, indent, iteratorSeed);
    }

    void printValues(INodePtr node, std::ostream& str, uint32_t level = 0) const
    {
        node->printValue(str, level);
    }
    std::string getValuesString(INodePtr str) const
    {
        std::ostringstream os;
        printValues(str, os, 0);
        return os.str();
    }

    void deserializeStructWithOffset(const void* src, size_t& offset, INodePtr structNode,
                                    int nesting = -1, INode::SDCallback sdCallback = {},
                                    const INode::Control& c = INode::Control(nullptr, nullptr)) const
    {
        std::stack<INode*> nodeStack;
        structNode->serializeOrDeserialize(src, nullptr, offset, false, nesting,
                                            nodeStack, c, sdCallback);
    }
    void serializeStructWithOffset(INodePtr structNode, void* dst, size_t& offset,
                                    int nesting = -1, INode::SDCallback sdCallback = {},
                                    const INode::Control& c = INode::Control(nullptr, nullptr)) const
    {
        std::stack<INode*> nodeStack;
        structNode->serializeOrDeserialize(nullptr, dst, offset, true, nesting,
                                            nodeStack, c, sdCallback);
    }
    void deserializeStruct(const void* src, INodePtr structNode,
                            int nesting = -1, INode::SDCallback sdCallback = {},
                            const INode::Control& = INode::Control(nullptr, nullptr)) const;
    void serializeStruct(INodePtr structNode, void* dst,
                        int nesting = -1, INode::SDCallback sdCallback = {},
                        const INode::Control& = INode::Control(nullptr, nullptr)) const;

    static bool composite(INodePtr node, add_cref<std::vector<uint8_t>> path, uint32_t depth,
                            add_ref<std::ostream> pathLog, add_ref<std::ostream> valueLog);

    bool selfTest() const;

private:
    void _getStructList(INodePtr rootStruct, std::vector<INodePtr>& list) const
    {
        INodePtr ch = rootStruct->getChildren(false);
        for (INodePtr p = ch->next; p; p = p->next)
        {
            if (INodePtr e = p->getElementType(); e && e->getChildren(false))
            {
                list.insert(list.begin(), e);
                _getStructList(e, list);
            }
            if (p->getChildren(false))
            {
                list.insert(list.begin(), p);
                _getStructList(p, list);
            }
        }
    }
    bool _compareTypes(INodePtr t1, INodePtr t2, add_ref<std::string> msg,
                        add_ref<std::string> lhsValue, add_ref<std::string> rhsValue,
                        bool compareValues) const;
    // --float --vec3 --mat2x3 -a 0:2 -a 1:2 -a 2:3 -s 1,2,101 -s 0,1,2,101 -a 200:2 -a 201:1 -a 200:3 -open-array-type 201:9 -a 201:9 -open-array-struct 102,200,100,0,104,101,2,105,201,103,1
};

} // namespace vtf

#endif // __VTF_STRUCT_GENERATOR_HPP_INCLUDED__
