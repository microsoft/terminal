#pragma once

namespace winrt
{
    using namespace ::winrt::Microsoft::Terminal::UI;
    using namespace ::winrt::Windows::Foundation;
    using namespace ::winrt::Windows::UI::Xaml;
    using namespace ::winrt::Windows::UI::Xaml::Shapes;
    using namespace ::winrt::Windows::UI::Xaml::Controls;
    using namespace ::winrt::Windows::UI::Xaml::Controls::Primitives;
    using namespace ::winrt::Windows::UI::Xaml::Input;
    using namespace ::winrt::Windows::UI::Xaml::Automation;
    using namespace ::winrt::Windows::UI::Xaml::Automation::Peers;
    using namespace ::winrt::Windows::ApplicationModel::DataTransfer;
    using FxScrollViewer = winrt::Windows::UI::Xaml::Controls::ScrollViewer;
    using PropertyChanged_revoker = winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker;
    namespace implementation = ::winrt::Microsoft::Terminal::UI::implementation;
}

#define CppWinRTActivatableClassWithFactory(className, factory) \
    namespace factory_implementation                            \
    {                                                           \
        using className = factory;                              \
    };                                                          \
    namespace implementation                                    \
    {                                                           \
        using className = ::className;                          \
    };

#define CppWinRTActivatableClass(className) \
    CppWinRTActivatableClassWithFactory(className, className##Factory)

#define CppWinRTActivatableClassWithBasicFactory(className)                                                         \
    struct className##Factory : public winrt::factory_implementation::className##T<className##Factory, ::className> \
    {                                                                                                               \
    };                                                                                                              \
    CppWinRTActivatableClassWithFactory(className, className##Factory)

#define CppWinRTActivatableClassWithDPFactory(className)                                                            \
    struct className##Factory : public winrt::factory_implementation::className##T<className##Factory, ::className> \
    {                                                                                                               \
        className##Factory() { EnsureProperties(); }                                                                \
        static void ClearProperties() { ::className::ClearProperties(); }                                           \
        static void EnsureProperties() { ::className::EnsureProperties(); }                                         \
    };                                                                                                              \
    CppWinRTActivatableClassWithFactory(className, className##Factory)

// Helper to use instead of winrt::DependencyProperty in a static global. Avoids getting a dynamic initializer and
// destructor, so it saves on dll size as well as avoids destructing a WinRT object at dll unload.
struct GlobalDependencyProperty
{
    GlobalDependencyProperty() = default;

    constexpr GlobalDependencyProperty(nullptr_t)
    {
    }

    // Can assign to nullptr which will free the DP.
    GlobalDependencyProperty& operator=(nullptr_t)
    {
        Property() = nullptr;
        return *this;
    }

    // Can also copy from winrt::DependencyProperty to transfer ownership for initialization.
    GlobalDependencyProperty& operator=(winrt::DependencyProperty const& other)
    {
        Property() = other;
        return *this;
    }

    GlobalDependencyProperty(winrt::DependencyProperty const& other)
    {
        Property() = other;
    }

    // Cannot copy or assign this helper because it is only for global static usage.
    GlobalDependencyProperty(GlobalDependencyProperty const&) = delete;
    GlobalDependencyProperty& operator=(GlobalDependencyProperty const& other) = delete;

    operator winrt::DependencyProperty() const
    {
        return Property();
    }

    operator bool() const
    {
        return static_cast<bool>(m_dependencyProperty);
    }

    bool operator==(winrt::DependencyProperty const& other) const
    {
        return Property() == other;
    }

    bool operator==(nullptr_t) const
    {
        return Property() == nullptr;
    }

private:
    winrt::DependencyProperty& Property()
    {
        return reinterpret_cast<winrt::DependencyProperty&>(m_dependencyProperty);
    }

    winrt::DependencyProperty const& Property() const
    {
        return reinterpret_cast<winrt::DependencyProperty const&>(m_dependencyProperty);
    }

    IUnknown* m_dependencyProperty{};
};

inline winrt::DependencyProperty InitializeDependencyProperty(
    std::wstring_view const& propertyNameString,
    std::wstring_view const& propertyTypeNameString,
    std::wstring_view const& ownerTypeNameString,
    bool isAttached,
    winrt::IInspectable const& defaultValue,
    winrt::PropertyChangedCallback const& propertyChangedCallback = nullptr)
{
    auto propertyType = winrt::Interop::TypeName();
    propertyType.Name = propertyTypeNameString;
    propertyType.Kind = winrt::Interop::TypeKind::Metadata;

    auto ownerType = winrt::Interop::TypeName();
    ownerType.Name = ownerTypeNameString;
    ownerType.Kind = winrt::Interop::TypeKind::Metadata;

    auto propertyMetadata = winrt::PropertyMetadata(defaultValue, propertyChangedCallback);

    if (isAttached)
    {
        return winrt::DependencyProperty::RegisterAttached(propertyNameString, propertyType, ownerType, propertyMetadata);
    }
    else
    {
        return winrt::DependencyProperty::Register(propertyNameString, propertyType, ownerType, propertyMetadata);
    }
}

// Helper to provide default values and boxing without differences at the call sites
template<typename T, typename Enable = void>
struct ValueHelper
{
    static T GetDefaultValue()
    {
#pragma warning(push)
#pragma warning(disable : 26444)
        return T{};
#pragma warning(pop)
    }

    static winrt::IInspectable BoxValueIfNecessary(T const& value)
    {
        return winrt::box_value(value);
    }

    static T CastOrUnbox(winrt::IInspectable const& value)
    {
        return winrt::unbox_value<T>(value);
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return BoxValueIfNecessary(GetDefaultValue());
    }
};

template<typename T>
struct ValueHelper<T, std::enable_if_t<std::is_base_of_v<winrt::Windows::Foundation::IUnknown, T>>>
{
    static T GetDefaultValue()
    {
        return T{ nullptr };
    }

    static winrt::IInspectable BoxValueIfNecessary(T const& value)
    {
        return value;
    }

    static T CastOrUnbox(winrt::IInspectable const& value)
    {
        return value.as<T>();
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return GetDefaultValue();
    }
};

template<>
struct ValueHelper<winrt::hstring, void>
{
    static winrt::hstring GetDefaultValue()
    {
        return winrt::hstring{ L"" };
    }

    static winrt::IInspectable BoxValueIfNecessary(winrt::hstring const& value)
    {
        return winrt::box_value(value);
    }

    static winrt::hstring CastOrUnbox(winrt::IInspectable const& value)
    {
        return winrt::unbox_value<winrt::hstring>(value);
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return winrt::box_value(L"");
    }
};

template<typename WinRTReturn>
WinRTReturn GetTemplateChildT(std::wstring_view const& childName, const winrt::IControlProtected& controlProtected)
{
    winrt::DependencyObject childAsDO(controlProtected.GetTemplateChild(childName));

    if (childAsDO)
    {
        return childAsDO.try_as<WinRTReturn>();
    }
    return nullptr;
}

template<typename T>
struct tracker_ref
{
    tracker_ref(void* discarded) {}
    T value;
    void set(const T& t) { value = t; }
    void set(T&& t) { value = std::move(t); }
    T get() { return value; }
};
