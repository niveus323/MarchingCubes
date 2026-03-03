#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

enum class EPropertyType
{
	Int,
	Float,
	Bool,
    Vector2,
	Vector3,
    String,
    Enum,
    Color
};

struct Property
{
	std::string name;
	EPropertyType type;
	size_t offset;
    std::string enumName;
    std::string group;
    bool isArray = false;

    std::function<size_t(void*)> getArraySize; // 배열 크기 Getter
    std::function<void* (void*, size_t)> getArrayElement; // 배열 원소 Getter
    std::function<void(void*, void*)> getter; // 접근용 Getter
    std::function<void(void*, const void*)> setter; // 수정용 Setter
    std::function<bool(void*)> isVisible; // 가시성 조건용 함수

    bool IsAccessor() const { return getter != nullptr; }
	void* GetValuePtr(void* instance) const { return static_cast<char*>(instance) + offset; }
};

class EnumDescriptor
{
public:
    struct Entry { int value; std::string name; };

    EnumDescriptor(const char* name) : m_name(name) {}

    void AddEntry(int value, const char* name) { m_entries.push_back({ value, name }); }
    const std::vector<Entry>& GetEntries() const { return m_entries; }
    const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    std::vector<Entry> m_entries;
};

class TypeDescriptor
{
public:
    TypeDescriptor(const char* name) : m_className(name) {}

    void AddProperty(const char* name, EPropertyType type, size_t offset, const char* enumName = "", const char* group = "", std::function<bool(void*)> condition = nullptr)
    {
        Property prop{
            .name = name,
            .type = type,
            .offset = offset,
            .enumName = enumName,
            .group = group,
            .isVisible = condition
        };
        m_properties.push_back(prop);
    }

    void AddPropertyAccessor(const char* name, 
        EPropertyType type, 
        std::function<void(void*, void*)> getter, 
        std::function<void(void*, const void*)> setter, 
        const char* enumName = "", 
        std::function<bool(void*)> condition = nullptr)
    {
        Property prop{
            .name = name,
            .type = type,
            .offset = 0, // Accessor는 offset 미사용
            .enumName = enumName,
            .getter = getter,
            .setter = setter,
            .isVisible = condition
        };
        m_properties.push_back(prop);
    }

    template<typename VectorType>
    void AddPropertyVector(const char* name, EPropertyType elementType, size_t offset, const char* group = "")
    {
        Property prop{
            .name = name,
            .type = elementType,
            .offset = offset,
            .group = group,
            .isArray = true
        };

        prop.getArraySize = [offset](void* instance) -> size_t {
            auto* vec = reinterpret_cast<VectorType*>(static_cast<char*>(instance) + offset);
            return vec->size();
        };

        prop.getArrayElement = [offset](void* instance, size_t index) -> void* {
            auto* vec = reinterpret_cast<VectorType*>(static_cast<char*>(instance) + offset);
            return &(*vec)[index];
        };
        m_properties.push_back(prop);
    }

    template<typename T>
    void SetConstructor()
    {
        if constexpr (!std::is_abstract_v<T>)
        {
            m_constructor = []() -> void* { return new T(); };
        }
        else
        {
            m_constructor = []() -> void* { return nullptr; };
        }
    }

    void* CreateInstance() const
    {
        if (m_constructor) return m_constructor();
        return nullptr;
    }

    bool IsDerivedFrom(const std::string& baseClassName) const
    {
        const TypeDescriptor* current = this;
        while (current)
        {
            if (current->GetName() == baseClassName)
            {
                return true;
            }
            current = current->GetParent();
        }
        return false;
    }

    TypeDescriptor* GetParent() const { return m_parent; }
    void SetParent(TypeDescriptor* parent) { m_parent = parent; }
    const std::vector<Property>& GetProperties() const { return m_properties; }
    const std::string& GetName() const { return m_className; }

private:
    std::string m_className;
    TypeDescriptor* m_parent = nullptr;
    std::vector<Property> m_properties;
    std::function<void*()> m_constructor;
};

class ReflectionRegistry
{
public:
    static ReflectionRegistry& Get() { static ReflectionRegistry instance; return instance; }

    void RegisterEnum(const char* name, EnumDescriptor* desc)
    {
        m_enums[name] = desc;
    }

    EnumDescriptor* GetEnum(const std::string& name)
    {
        if (m_enums.find(name) != m_enums.end()) return m_enums[name];
        return nullptr;
    }

    void RegisterType(const std::string& name, TypeDescriptor* desc)
    {
        m_types[name] = desc;
    }

    TypeDescriptor* GetType(const std::string& name)
    {
        if (m_types.find(name) != m_types.end()) return m_types[name];
        return nullptr;
    }
    
    const std::map<std::string, TypeDescriptor*>& GetTypes() const { return m_types; }

    std::vector<TypeDescriptor*> GetTypesDerivedFrom(const std::string& baseClassName) const
    {
        std::vector<TypeDescriptor*> result;
        for (const auto& [name, desc] : m_types)
        {
            if (desc->IsDerivedFrom(baseClassName))
            {
                result.push_back(desc);
            }
        }
        return result;
    }

private:
    std::map<std::string, EnumDescriptor*> m_enums;
    std::map<std::string, TypeDescriptor*> m_types;
};

// 씬 직렬화 및 씬 로드 시 클래스가 리플렉션에 등록되어 있지 않으면 로드할 수 없다 -> static으로 등록기를 선언하여 클래스 로드 시점을 프로그램 실행 시점으로 옮김
template<typename T>
struct ClassRegisterar
{
    ClassRegisterar(const char* name)
    {
        TypeDescriptor* desc = T::GetStaticType(); // 타입 추론
        desc->SetConstructor<T>(); // 기본 생성자 호출
        ReflectionRegistry::Get().RegisterType(name, desc); // Registry에 등록
    }
};

// --- Reflection 매크로 ---
// .h 클래스 내부에 GetStaticType, GetType 선언
#define REFLECT_GENERATED_BODY(Class) \
public: \
    static TypeDescriptor* GetStaticType(); \
    virtual TypeDescriptor* GetType() const { return GetStaticType(); } \
    friend class TypeDescriptor; \
    friend struct ClassRegisterar<Class>; \
    Class() = default; 

// Enum 클래스용
#define BEGIN_ENUM_REFLECTION(EnumName) \
    class EnumRegistrar_##EnumName { \
    public: \
        EnumRegistrar_##EnumName() { \
            static EnumDescriptor desc(#EnumName); \
            ReflectionRegistry::Get().RegisterEnum(#EnumName, &desc); \
            EnumDescriptor* d = &desc;

// Enum 원소 등록
#define REFLECT_ENUM(Value) d->AddEntry((int)Value, #Value);

// Enum 클래스 등록 종료
#define END_ENUM_REFLECTION(EnumName) } }; static EnumRegistrar_##EnumName global_##EnumName##_reg;

// 부모 클래스가 없는 루트 클래스용
#define BEGIN_REFLECTION_ROOT(Class) \
    TypeDescriptor* Class::GetStaticType() \
    { \
        using ThisClass = Class; \
        static TypeDescriptor typeDesc(#Class); \
        static bool isInitialized = false; \
        if (!isInitialized) \
        { \
            isInitialized = true;

// 부모 클래스가 있는 클래스용
#define BEGIN_REFLECTION(Class, Parent) \
    static ClassRegisterar<Class> global_##Class##_reg(#Class); \
    BEGIN_REFLECTION_ROOT(Class)\
            typeDesc.SetParent(Parent::GetStaticType());

// 클래스 등록 종료
#define END_REFLECTION() \
        } \
        return &typeDesc; \
    }

// 프로퍼티 등록
#define REFLECT_PROPERTY(Name, Type) \
            typeDesc.AddProperty(#Name, Type, offsetof(ThisClass, Name));

// 배열(std::vector) 프로퍼티 등록
#define REFLECT_PROPERTY_ARRAY(Name, ElementType) \
    typeDesc.AddPropertyVector<decltype(ThisClass::Name)>(#Name, ElementType, offsetof(ThisClass, Name), #Name);

// Enum 프로퍼티 등록
#define REFLECT_PROPERTY_ENUM(Name, EnumType) \
    typeDesc.AddProperty(#Name, EPropertyType::Enum, offsetof(ThisClass, Name), #EnumType);

// Getter/Setter 등록
#define REFLECT_PROPERTY_FN(Name, EnumType, CppType, GetterFunc, SetterFunc) \
    typeDesc.AddPropertyAccessor(Name, EnumType, \
        [](void* inst, void* outVal) { \
            *static_cast<CppType*>(outVal) = static_cast<ThisClass*>(inst)->GetterFunc(); \
        }, \
        [](void* inst, const void* inVal) { \
            static_cast<ThisClass*>(inst)->SetterFunc(*static_cast<const CppType*>(inVal)); \
        } \
    );

// Enum용 Getter/Setter (int 캐스팅 필요)
#define REFLECT_PROPERTY_ENUM_FN(Name, EnumName, GetterFunc, SetterFunc) \
    typeDesc.AddPropertyAccessor(Name, EPropertyType::Enum, \
        [](void* inst, void* outVal) { \
            *static_cast<int*>(outVal) = (int)static_cast<ThisClass*>(inst)->GetterFunc(); \
        }, \
        [](void* inst, const void* inVal) { \
            static_cast<ThisClass*>(inst)->SetterFunc((EnumName)*static_cast<const int*>(inVal)); \
        }, \
        #EnumName \
    );

// 조건부 일반 프로퍼티 (ConditionFunc가 true일 때만 표시)
#define REFLECT_PROPERTY_IF(Name, Type, ConditionFunc) \
            typeDesc.AddProperty(#Name, Type, offsetof(ThisClass, Name), "", "",\
            [](void* inst) { return static_cast<ThisClass*>(inst)->ConditionFunc(); });

// 조건부 일반 프로퍼티 (람다형)
#define REFLECT_PROPERTY_EXPR_IF(Name, Type, Expr) \
    typeDesc.AddProperty(#Name, Type, offsetof(ThisClass, Name), "", "",\
    [](void* voidInst) { \
        ThisClass* inst = static_cast<ThisClass*>(voidInst); \
        return (Expr); \
    });

// 조건부 Accessor 프로퍼티
#define REFLECT_PROPERTY_FN_IF(Name, EnumType, CppType, GetterFunc, SetterFunc, ConditionFunc) \
    typeDesc.AddPropertyAccessor(Name, EnumType, \
        [](void* inst, void* outVal) { *static_cast<CppType*>(outVal) = static_cast<ThisClass*>(inst)->GetterFunc(); }, \
        [](void* inst, const void* inVal) { static_cast<ThisClass*>(inst)->SetterFunc(*static_cast<const CppType*>(inVal)); }, \
        "", \
        [](void* inst) { return static_cast<ThisClass*>(inst)->ConditionFunc(); } \
    );

// 조건부 Accessor 프로퍼티 (람다형)
#define REFLECT_PROPERTY_FN_EXPR_IF(Name, EnumType, CppType, GetterFunc, SetterFunc, Expr) \
    typeDesc.AddPropertyAccessor(Name, EnumType, \
        [](void* inst, void* outVal) { *static_cast<CppType*>(outVal) = static_cast<ThisClass*>(inst)->GetterFunc(); }, \
        [](void* inst, const void* inVal) { static_cast<ThisClass*>(inst)->SetterFunc(*static_cast<const CppType*>(inVal)); }, \
        "", \
        [](void* voidInst) { \
            ThisClass* inst = static_cast<ThisClass*>(voidInst); \
            return (Expr); \
        } \
    );