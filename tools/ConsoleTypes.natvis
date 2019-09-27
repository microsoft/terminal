
<AutoVisualizer xmlns="http://schemas.microsoft.com/vstudio/debugger/natvis/2010">
    <!-- See https://docs.microsoft.com/en-us/visualstudio/debugger/create-custom-views-of-native-objects?view=vs-2017#BKMK_Syntax_reference for documentation -->
    <Type Name="TextColor">
        <DisplayString Condition="_meta==ColorType::IsIndex">{{Index:{_index}}}</DisplayString>
        <DisplayString Condition="_meta==ColorType::IsDefault">{{Default}}</DisplayString>
        <DisplayString Condition="_meta==ColorType::IsRgb">{{RGB:{_red},{_green},{_blue}}}</DisplayString>
        <Expand></Expand>
    </Type>

    <Type Name="TextAttribute">
        <!-- You can't do too much trickyness inside the DisplayString format
                string, so we'd have to add entries for each flag if we really
                wanted them to show up like that. -->
        <DisplayString Condition="_isBold">{{FG:{_foreground},BG:{_background},{_wAttrLegacy}, Bold}}</DisplayString>
        <DisplayString>{{FG:{_foreground},BG:{_background},{_wAttrLegacy}, Normal}}</DisplayString>
        <Expand>
            <Item Name="_wAttrLegacy">_wAttrLegacy</Item>
            <Item Name="_isBold">_isBold</Item>
            <Item Name="_foreground">_foreground</Item>
            <Item Name="_background">_background</Item>
        </Expand>
    </Type>

    <Type Name="Microsoft::Console::Types::Viewport">
        <!-- Can't call functions in here -->
        <DisplayString>{{LT({_sr.Left}, {_sr.Top}) RB({_sr.Right}, {_sr.Bottom}) [{_sr.Right-_sr.Left+1} x { _sr.Bottom-_sr.Top+1}]}}</DisplayString>
        <Expand>
            <ExpandedItem>_sr</ExpandedItem>
        </Expand>
    </Type>

    <Type Name="_COORD">
        <DisplayString>{{{X},{Y}}}</DisplayString>
    </Type>

    <Type Name="_SMALL_RECT">
        <DisplayString>{{LT({Left}, {Top}) RB({Right}, {Bottom}) In:[{Right-Left+1} x {Bottom-Top+1}] Ex:[{Right-Left} x {Bottom-Top}]}}</DisplayString>
    </Type>

    <Type Name="CharRowCell">
        <DisplayString Condition="_attr._glyphStored">Stored Glyph, go to UnicodeStorage.</DisplayString>
        <DisplayString Condition="_attr._attribute == 0">{_wch,X} Single</DisplayString>
        <DisplayString Condition="_attr._attribute == 1">{_wch,X} Lead</DisplayString>
        <DisplayString Condition="_attr._attribute == 2">{_wch,X} Trail</DisplayString>
    </Type>

    <Type Name="ATTR_ROW">
        <DisplayString>{{ size={_cchRowWidth} }}</DisplayString>
        <Expand>
            <ExpandedItem>_list</ExpandedItem>
        </Expand>
    </Type>

    <Type Name="CharRow">
        <DisplayString>{{ wrap={_wrapForced} padded={_doubleBytePadded} }}</DisplayString>
        <Expand>
            <ExpandedItem>_data</ExpandedItem>
        </Expand>
    </Type>

    <Type Name="ROW">
        <DisplayString>{{ id={_id} width={_rowWidth} }}</DisplayString>
        <Expand>
            <Item Name="_charRow">_charRow</Item>
            <Item Name="_attrRow">_attrRow</Item>
        </Expand>
    </Type>

    <Type Name="std::unique_ptr&lt;TextBuffer,std::default_delete&lt;TextBuffer&gt;&gt;">
        <Expand>
            <ExpandedItem>_Mypair._Myval2</ExpandedItem>
        </Expand>
    </Type>

    <Type Name="KeyEvent">
        <DisplayString Condition="_keyDown">{{↓  wch:{_charData} mod:{_activeModifierKeys} repeat:{_repeatCount} vk:{_virtualKeyCode} vsc:{_virtualScanCode}}</DisplayString>
        <DisplayString Condition="!_keyDown">{{↑ wch:{_charData} mod:{_activeModifierKeys} repeat:{_repeatCount} vk:{_virtualKeyCode} vsc:{_virtualScanCode}}</DisplayString>
    </Type>
</AutoVisualizer>
