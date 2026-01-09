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
    virtual INodePtr clone(bool raise = true) const {
        if (raise) {
            const std::string msg = "No overriden " + typeName + "::clone() found";
            throw std::runtime_error(msg);
        }
		return nullptr;
	}
    struct Cmp {
        std::string result;
        INodePtr comparedNode;
        uint32_t index = 0u;
    };
    virtual bool isArray(add_ref<std::size_t> /*elemCount*/) const { return false; }
    virtual void cmp(INodePtr /*thiz*/, INodePtr /*other*/, uint32_t /*fieldIndex*/,
                         add_ref<std::stack<Cmp>>, add_ref<int> result) {
        result = (-1);
        throw std::runtime_error("No overriden " + typeName + "::cmp() found");
    }
    virtual void loop(float& /*seedValue*/) { }
    virtual std::string genFieldName(uint32_t index, bool /*appendRank*/ = false) const {
        return typeName + '_' + std::to_string(index);
    }
    virtual void printValue(std::ostream&, uint32_t) const {}
    virtual std::size_t getBaseAlignment() const {
        ASSERT(false, "getBaseAlignment() not defined");
        return 0u;
    }
    virtual std::size_t getLogicalSize() const { return 0; }
    enum class SDAction : uint32_t { None, Serialize, Deserialize, UpdatePadBefore, UpdatePadAfter };
    struct SDParams
    {
        std::size_t alignment;
        std::size_t size;
        std::size_t offset;
        std::size_t pad;
        uint32_t    fieldIndex;
        int         nesting;
        SDAction    action;
        SDAction    whenAction;
        std::string fieldType;
    };
    using SDCallback = std::function<void(const SDParams&)>;
    virtual void serializeOrDeserialize(const void* /*src*/, void* /*dst*/, std::size_t& /*offset*/,
                                        bool /*serialize*/, bool /*arrayElement*/, int& /*nesting*/, SDCallback) {
    }
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
	virtual void printValue(std::ostream& str, uint32_t level) const override
	{
		str << typeName << ' ' << typeName << level << " = " << value << ' ';
	}
    virtual INodePtr clone(bool) const override {
		return std::make_shared<Node<X>>(type_to_string<X>, nullptr);
	}
    virtual void loop(float& seed) override {
		value = X(seed);
        seed += 1.0f;
	}
    virtual void cmp(INodePtr thiz, INodePtr other, uint32_t fieldIndex,
                     add_ref<std::stack<Cmp>> nodeStack, add_ref<int> result) override {
        int status = 0;
        Cmp c{ std::string(), thiz, 0u };
        std::stringstream os;
        if (typeName != other->typeName) {
            std::size_t elemCount = 0u;
            printValue(os, fieldIndex);
            os << "compared with " << other->typeName;
            if (other->isArray(elemCount))
            {
                os << '[' << elemCount << ']';
            }
            status = (-1);
        }
        else
        {
            const X otherValue = std::static_pointer_cast<Node>(other)->value;
            if (otherValue != value)
            {
                os << genFieldName(fieldIndex) << " is " << value << ", expected " << otherValue;
                status = +1;
            }
        }
        if ((result = status))
        {
            c.result = os.str();
            nodeStack.push(c);
        }
    }

    virtual std::size_t getBaseAlignment() const override {
        return 4u;
    }
    virtual std::size_t getLogicalSize() const override {
        return sizeof(X);
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& offset,
                                        bool serialize, bool /*arrayElement*/, int& /*nesting*/, SDCallback) override
    {
        if (serialize)
        {
            X* p = reinterpret_cast<X*>(reinterpret_cast<std::byte*>(dst) + offset);
            *p = value;
        }
        else
        {
            const X* p = reinterpret_cast<const X*>(reinterpret_cast<const std::byte*>(src) + offset);
            value = *p;
        }
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

template<class X, std::size_t Elems, bool IsRuntime> struct Array
{
    INodePtr structure;
    //template<class Y = X, std::enable_if_t<!std::is_same_v<Y, INodePtr>, int> = 0>
    Array() : structure() {}
    //template<class Y, std::enable_if_t<std::is_same_v<Y, INodePtr>, int> = 1>
    //Array(Y structureNode) : structure(structureNode) {}
};
template<std::size_t Elems, bool IsRuntime> struct Array<INodePtr, Elems, IsRuntime>
{
    INodePtr structure;
    //template<class Y = X, std::enable_if_t<!std::is_same_v<Y, INodePtr>, int> = 0>
    //Array() : structure() {}
    //template<class Y, std::enable_if_t<std::is_same_v<Y, INodePtr>, int> = 1>
    Array(INodePtr structureNode) : structure(structureNode) {}
};

template<class X, std::size_t Elems, bool IsRuntime>
struct Node<Array<X, Elems, IsRuntime>> : public INode
{
    static_assert(false == std::is_same_v<X, INodePtr>,
        "In Node<Array<X, Elems, IsRuntime>> the X must not be of INodePtr");

    X value[Elems];
    using ValueType = X[Elems];
    Node(const std::string& /*typeName*/, INodePtr)
        : INode(type_to_string<X>, nullptr)
    {
    }
    virtual INodePtr clone(bool) const override {
        return std::make_shared<Node>(typeName, nullptr);
    }
    virtual std::string genFieldName(uint32_t index, bool appendRank) const override {
        const std::string fieldName = INode::genFieldName(index);
        if (appendRank)
            return fieldName + '[' + (IsRuntime ? ("/* " + std::to_string(Elems) + " */") : std::to_string(Elems)) + ']';
        return fieldName;
    }
    virtual INodePtr getElementType() const override {
        return std::make_shared<Node<X>>(std::string(), nullptr);
    }
    virtual void printValue(std::ostream& str, uint32_t level) const override {
		str << typeName << ' ' << typeName << level << '[' << Elems << "] = ";
		for (std::size_t i = 0; i < Elems; ++i)
		{
            str << '[' << i << "](" << value[i] << ")";
		}
        str << ' ';
	}
    virtual void loop(float& seed) override {
        for (std::size_t i = 0; i < Elems; ++i) {
            value[i] = X(seed);
            seed += 1;
        }
    }
    virtual void cmp(INodePtr thiz, INodePtr other, uint32_t fieldIndex,
        add_ref<std::stack<Cmp>> nodeStack, add_ref<int> result) override {
        int status = 0;
        Cmp c{ std::string(), thiz, 0u };
        std::stringstream os;
        std::size_t otherElemCount = 0u;
        const bool otherIsArray = other->isArray(otherElemCount);
        if ((false == (typeName == other->typeName)) || (otherIsArray && (Elems == otherElemCount))) {
            os << genFieldName(fieldIndex, true) <<  " compared with " << other->typeName;
            if (otherIsArray)
            {
                os << '[' << otherElemCount << ']';
            }
            status = (-1);
        }
        else
        {
            add_cref<ValueType> otherValue = std::static_pointer_cast<Node>(other)->value;
            for (uint32_t i = 0u; i < Elems; ++i)
            {
                if (otherValue[i] != value[i])
                {
                    os << typeName << genFieldName(fieldIndex) << '[' << i << ']'
                        << " is " << value[i] << ", expected " << otherValue[i];
                    status = (-1);
                    break;
                }
            }
        }
        if ((result = status))
        {
            c.result = os.str();
            nodeStack.push(c);
        }
    }

    virtual std::size_t getBaseAlignment() const override {
        return getElementType()->getBaseAlignment();
    }
    virtual std::size_t getLogicalSize() const override {
        return Elems * getBaseAlignment();
    }
    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& offset,
                                        bool serialize, bool /*arrayElement*/,
                                        int& /*nesting*/, SDCallback) override
    {
        const std::size_t stride = getBaseAlignment();
        std::size_t rwOffset = offset;
        for (std::size_t i = 0; i < Elems; ++i)
        {
            if (serialize)
            {
                X* p = reinterpret_cast<X*>(reinterpret_cast<std::byte*>(dst) + rwOffset);
                *p = value[i];
            }
            else
            {
                const X* p = reinterpret_cast<const X*>(reinterpret_cast<const std::byte*>(src) + rwOffset);
                value[i] = *p;
            }

            rwOffset += stride;
        }
    }
    virtual uint32_t getVisitCount() const override {
        return uint32_t(Elems);
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
        const std::string path = currentPath + '[' + it + ']';
        putIndent(str, indent);
        str << "for (uint " << it << " = 0; " << it << " < " << Elems << "; ++" << it << ") {\n";
        putIndent(str, indent + 1u);
        str << path << " = " << getElementType()->typeName << '(' << rhsCode << ");\n";
        putIndent(str, indent);
        str << "}\n";
    }
};

template<std::size_t Elems, bool IsRuntime>
struct Node<Array<INodePtr, Elems, IsRuntime>> : public INode
{
    std::vector<INodePtr> value;
    const std::size_t dynamicElems;
    const bool dynamicIsRuntime;
    Node(const std::string& /*typeName*/, INodePtr structure) 
        : INode(structure->typeName, nullptr)
        , value(Elems)
        , dynamicElems(Elems)
        , dynamicIsRuntime(IsRuntime)
    {
        for (uint32_t i = 0u; i < Elems; ++i)
            value[i] = structure->clone();
    }
    Node(INodePtr elemType, std::size_t elemCount, bool isRuntimeArray = false)
        : INode(elemType->typeName, nullptr)
        , value(elemCount)
        , dynamicElems(elemCount)
        , dynamicIsRuntime(isRuntimeArray)
    {
        for (uint32_t i = 0u; i < elemCount; ++i)
            value[i] = elemType->clone();
    }
    virtual INodePtr clone(bool) const override {
        return std::make_shared<Node<Array<INodePtr, 1, false>>>(value[0], dynamicElems, dynamicIsRuntime);
    }
    virtual INodePtr getElementType() const override {
        return value[0];
    }
    virtual std::string genFieldName(uint32_t index, bool appendRank) const override {
        const std::string fieldName = INode::genFieldName(index);
        if (appendRank)
            return fieldName + '[' + (dynamicIsRuntime
                                        ? ("/* " + std::to_string(dynamicElems) + " */")
                                        : std::to_string(dynamicElems)) + ']';
        return fieldName;
    }
    virtual void printValue(std::ostream& str, uint32_t level) const override {
        str << typeName << ' ' << typeName << level << '[' << dynamicElems << "] = ";
        for (std::size_t i = 0; i < dynamicElems; ++i)
        {
            str << '[' << i << "](";
            value[i]->printValue(str, level+1);
            str << ") ";
        }
    }
    virtual void loop(float& seed) override {
        for (std::size_t i = 0; i < dynamicElems; ++i) {
            value[i]->loop(seed);
        }
    }
    virtual std::size_t getBaseAlignment() const override {
        return value[0]->getBaseAlignment();
    }
    virtual std::size_t getLogicalSize() const override {
        const std::size_t elemAb = value[0]->getBaseAlignment();
        const std::size_t alignedElemSize = ((value[0]->getLogicalSize() + elemAb - 1u) / elemAb) * elemAb;
        return dynamicElems * alignedElemSize;
    }
    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& offset,
                                        bool serialize, bool /*arrayElement*/, int& nesting, SDCallback sdCallback) override
    {
        const std::size_t stride = value[0]->getLogicalSize();

        std::size_t rwOffset = offset;
        for (std::size_t i = 0; i < dynamicElems; ++i)
        {
            std::size_t elementOffset = 0u;

            if (serialize) {
                std::byte* elementAddr = reinterpret_cast<std::byte*>(dst) + rwOffset;
                value[i]->serializeOrDeserialize(nullptr, elementAddr, elementOffset, true, true, nesting, sdCallback);
            } 
            else {
                const std::byte* elementAddr = reinterpret_cast<const std::byte*>(src) + rwOffset;
                value[i]->serializeOrDeserialize(elementAddr, nullptr, elementOffset, false, true, nesting, sdCallback);
            }
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
    T value[Cols * Rows]{};

    virtual INodePtr clone(bool) const override {
        return std::make_shared<Node<mat<Cols, Rows, T, ColumnMajor>>>(genTypeName(), nullptr);
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

    virtual void printValue(std::ostream& str, uint32_t level) const override {
        str << typeName << ' ' << typeName << '_' << level << " = ";
        for (std::size_t C = 0u; C < Cols; ++C) {
            for (std::size_t R = 0u; R < Rows; ++R) {
                str << '[' << C << ',' << R << "](" << value[C * Rows + R] << ")";
            }
        }
        str << ' ';
    }

    virtual INodePtr getElementType() const override {
        return std::make_shared<Node<T>>(std::string(), nullptr);
    }

    virtual void loop(float& seed) override {
        for (std::size_t C = 0u; C < Cols; ++C) {
            for (std::size_t R = 0u; R < Rows; ++R) {
                value[C * Rows + R] = T(seed);
                seed += 1.f;
            }
        }
    }

    virtual std::size_t getBaseAlignment() const override {
        return getStride();
    }
    std::size_t getStride() const
    {
        constexpr std::size_t vecSize = ColumnMajor ? Rows : Cols;
        const std::size_t elemSize = getElementType()->getLogicalSize();
        return (vecSize == 3u) ? (4u * elemSize) : (vecSize * elemSize);
    }
    virtual std::size_t getLogicalSize() const override {
        return (ColumnMajor ? Cols : Rows) * getStride();
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& xoffset, bool serialize,
                                        bool /*arrayElement*/, int& /*nesting*/, SDCallback) override
    {
        std::size_t rwOffset = xoffset;
        constexpr std::size_t iMax = ColumnMajor ? Cols : Rows;
        constexpr std::size_t jMax = ColumnMajor ? Rows : Cols;

        for (std::size_t i = 0u; i < iMax; ++i)
        {
            std::size_t vecOffset = rwOffset;

            for (std::size_t j = 0u; j < jMax; ++j)
            {
                if (serialize)
                {
                    T* p = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(dst) + vecOffset);
                    *p = value[i * jMax + j];
                }
                else
                {
                    const T* p = reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(src) + vecOffset);
                    value[i * jMax + j] = *p;
                }
                vecOffset = vecOffset + sizeof(T);
            }
            rwOffset = rwOffset + getStride();
        }
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

    virtual INodePtr clone(bool) const override {
        return std::make_shared<Node<vec<K, T>>>(genTypeName(), nullptr);
    }

    std::string genTypeName() const {
        std::ostringstream os;
        os << (std::is_unsigned_v<T> ? "u" : "")
           << (std::is_integral_v<T> ? "i" : "")
           << "vec" << K;
        return os.str();
    }

    Node(const std::string&, INodePtr) : INode(genTypeName(), nullptr) {}

    virtual void printValue(std::ostream& str, uint32_t level) const override {
        str << typeName << ' ' << typeName << level << " = ";
        for (std::size_t X = 0u; X < K; ++X) {
            str << '[' << X << "](" << value[X] << ")";
        }
        str << ' ';
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

    virtual void cmp(INodePtr thiz, INodePtr other, uint32_t fieldIndex,
        add_ref<std::stack<Cmp>> nodeStack, add_ref<int> result) override {
        int status = 0;
        Cmp c{ std::string(), thiz, 0u };
        std::stringstream os;
        if (typeName != other->typeName) {
            std::size_t elemCount = 0u;
            printValue(os, fieldIndex);
            os << "compared with " << other->typeName;
            if (other->isArray(elemCount))
            {
                os << '[' << elemCount << ']';
            }
            status = (-1);
        }
        else
        {
            add_cref<ValueType> otherValue = std::static_pointer_cast<Node>(other)->value;
            for (uint32_t k = 0u; k < K; ++k) {
                if (otherValue[k] != value[k]) {
                    std::ostringstream valueStream, otherValueStream;
                    valueStream << '['; otherValueStream << '[';
                    for (uint32_t i = 0; i < K; ++i) {
                        if (i) {
                            valueStream << ", "; otherValueStream << ", ";
                        }
                        valueStream << value[i]; otherValueStream << otherValue[i];
                    }
                    valueStream << ']'; otherValueStream << ']';
                    os << genFieldName(fieldIndex) << " is " << valueStream.str()
                        << ", expected " << otherValueStream.str();
                    status = (-1);
                    break;
                }
            }
        }
        if ((result = status))
        {
            c.result = os.str();
            nodeStack.push(c);
        }
    }

    virtual std::size_t getBaseAlignment() const override {
        const std::size_t elemSize = getElementType()->getBaseAlignment();
        return (K == 3u) ? (4u * elemSize) : (K * elemSize);
    }

    virtual std::size_t getLogicalSize() const override {
        return getBaseAlignment();
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& offset, bool serialize,
                                        bool /*arrayElement*/, int& /*nesting*/, SDCallback) override
    {
        std::size_t elemOffset = offset;
        for (std::size_t i = 0u; i < K; ++i)
        {
            if (serialize)
            {
                T* p = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(dst) + elemOffset);
                *p = value[i];
            }
            else
            {
                const T* p = reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(src) + elemOffset);
                value[i] = *p;
            }
            elemOffset = elemOffset + sizeof(T);
        }
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

template<> struct Node<INodePtr> : public INode
{
	INodePtr children;
	Node(const std::string&, INodePtr ch)
		: INode(ch->typeName, ch)
		, children(cloneChildren(ch))
	{}
    virtual INodePtr getChildren(bool) const override {
		return children;
	}
	virtual void printValue(std::ostream& str, uint32_t level) const override
	{
		str << typeName << ' ' << typeName << level << " { ";
		for (INodePtr p = children->next; p; p = p->next)
			p->printValue(str, (level + 1));
		str << "} ";
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
    virtual INodePtr clone(bool) const override {
		return std::make_shared<Node>(typeName, children);
	}
    virtual void loop(float& seed) override {
		for (INodePtr p = children->next; p; p = p->next)
            p->loop(seed);
	}
    virtual std::size_t getBaseAlignment() const override {
        size_t maxAb = 4u;
        for (INodePtr p = children->next; p; p = p->next) {
            maxAb = std::max(maxAb, p->getBaseAlignment());
        }
        return maxAb;
    }
    std::size_t updateOffsetWithPadding(const INode* field, std::size_t baseAlignment, std::size_t& offset) const
    {
        const std::size_t Ab = field ? field->getBaseAlignment() : baseAlignment;
        const std::size_t alignedOffset = ((offset + Ab - 1u) / Ab) * Ab;
        const std::size_t padding = alignedOffset - offset;
        offset += padding;
        return padding;
    }
    virtual std::size_t getLogicalSize() const override
    {
        size_t tempOffset = 0;

        for (INodePtr p = children->next; p; p = p->next) {
            const std::size_t size = p->getLogicalSize();
            updateOffsetWithPadding(p.get(), 1u, tempOffset);
            tempOffset = tempOffset + size;
        }

        updateOffsetWithPadding(this, 1u, tempOffset);
        return tempOffset;
    }

    virtual void serializeOrDeserialize(const void* src, void* dst, std::size_t& offset, bool serialize,
                                        bool /*arrayElement*/, int& nesting, SDCallback sdCallback) override
    {
        nesting = nesting + 1;

        const std::size_t thisStructAb = getBaseAlignment();
        const std::size_t thisStructSize = sdCallback ? getLogicalSize() : 1u;
        const std::size_t initialPadding = updateOffsetWithPadding(nullptr, thisStructAb, offset);
        if (initialPadding && sdCallback)
        {
            SDParams sdp;
            sdp.action      = SDAction::UpdatePadBefore;
            sdp.whenAction  = serialize ? SDAction::Serialize : SDAction::Deserialize;
            sdp.alignment   = thisStructAb;
            sdp.fieldIndex  = 9999;
            sdp.fieldType   = typeName;
            sdp.nesting     = nesting;
            sdp.offset      = (offset - initialPadding);
            sdp.pad         = initialPadding;
            sdp.size        = thisStructSize;
            sdCallback(sdp);
        }

        uint32_t childIndex = 0u;

        for (INodePtr p = children->next; p; p = p->next, ++childIndex) {
            const std::size_t fieldAlignment = p->getBaseAlignment();
            const std::size_t fieldLogicalSize = p->getLogicalSize();
            const bool insideStructure = !!p->getChildren(false);
            if (false == insideStructure)
            {
                const std::size_t pad = updateOffsetWithPadding(nullptr, fieldAlignment, offset);
                if (pad && sdCallback)
                {
                    SDParams sdp;
                    sdp.action      = SDAction::UpdatePadBefore;
                    sdp.whenAction  = serialize ? SDAction::Serialize : SDAction::Deserialize;
                    sdp.alignment   = fieldAlignment;
                    sdp.fieldIndex  = childIndex;
                    sdp.fieldType   = typeName + "::" + p->genFieldName(childIndex, true);
                    sdp.nesting     = nesting;
                    sdp.offset      = (offset - pad);
                    sdp.pad         = pad;
                    sdp.size        = fieldLogicalSize;
                    sdCallback(sdp);
                }
            }
            if (sdCallback)
            {
                SDParams sdp;
                sdp.action      = serialize ? SDAction::Serialize : SDAction::Deserialize;
                sdp.whenAction  = sdp.action;
                sdp.alignment   = fieldAlignment;
                sdp.fieldIndex  = childIndex;
                sdp.fieldType = typeName + "::" + p->genFieldName(childIndex, true);
                sdp.nesting     = 0;
                sdp.offset      = offset;
                sdp.pad         = 0;
                sdp.size        = fieldLogicalSize;
                sdCallback(sdp);
            }
            p->serializeOrDeserialize(src, dst, offset, serialize, false, nesting, sdCallback);
            if (false == insideStructure)
            {
                offset = offset + fieldLogicalSize;
            }
        }

        const std::size_t finalPadding = updateOffsetWithPadding(nullptr, thisStructAb, offset);
        if (finalPadding && sdCallback)
        {
            SDParams sdp;
            sdp.action      = SDAction::UpdatePadAfter;
            sdp.whenAction  = serialize ? SDAction::Serialize : SDAction::Deserialize;
            sdp.alignment   = thisStructAb;
            sdp.fieldIndex  = 9999;
            sdp.fieldType   = typeName;
            sdp.nesting     = nesting;
            sdp.offset      = (offset - initialPadding);
            sdp.pad         = initialPadding;
            sdp.size        = thisStructSize;
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

} //namespace sg

struct StructGenerator
{
    using INode = sg::INode;
    using INodePtr = sg::INodePtr;
    template<typename X> using Node = sg::Node<X>;

    INodePtr generateStruct(const std::string& typeName, const std::vector<INodePtr> fields)
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
    template<class FieldType> INodePtr makeField(const FieldType&)
    {
        return std::make_shared<Node<FieldType>>(std::string(), nullptr);
    }

    INodePtr makeArrayField(INodePtr elemType, std::size_t elemCount, bool isRuntime = false)
    {
        return std::make_shared<Node<sg::Array<INodePtr, 1, false>>>(elemType, elemCount, isRuntime);
    }

    bool structureAppendField(INodePtr rootStruct, INodePtr field)
    {
        INodePtr lastField = rootStruct->getLastChild(false);
        if (lastField)
        {
            lastField->next = field->clone();
            return true;
        }
        return false;
    }

    void printStruct(INodePtr structNode, std::ostream& str, bool declaration = true)
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

    std::vector<INodePtr> getStructList(INodePtr rootStruct)
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

    void generateLoops(INodePtr rootStruct, const std::string& rootStructName, std::ostream& str, uint32_t indent, const std::string& rhsCode)
    {
        uint32_t iteratorSeed = 0;
        rootStruct->genLoops(str, rootStructName, rhsCode, 0, indent, iteratorSeed);
    }

    void printValues(INodePtr node, std::ostream& str, uint32_t level = 0)
    {
        node->printValue(str, level);
    }
    std::string getValuesString(INodePtr str)
    {
        std::ostringstream os;
        printValues(str, os, 0);
        return os.str();
    }

    void deserializeStruct(const void* src, INodePtr structNode, int nesting = -1, INode::SDCallback sdCallback = {})
    {
        std::size_t currentOffset = 0;
        structNode->serializeOrDeserialize(src, nullptr, currentOffset, false, false, nesting, sdCallback);
    }
    void serializeStruct(INodePtr structNode, void* dst, int nesting = -1, INode::SDCallback sdCallback = {})
    {
        std::size_t currentOffset = 0;
        structNode->serializeOrDeserialize(nullptr, dst, currentOffset, true, false, nesting, sdCallback);
    }

private:
    void _getStructList(INodePtr rootStruct, std::vector<INodePtr>& list)
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
};

} // namespace vtf

#endif // __VTF_STRUCT_GENERATOR_HPP_INCLUDED__
