#include "pch.h"
#include "NewTabMenuEntry.h"
//#include "SeparatorEntry.h"

#include "NewTabMenuEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

NewTabMenuEntry::NewTabMenuEntry(const NewTabMenuEntryType type) noexcept :
    _Type{ type }
{
}
