namespace tracy
{

#if defined __linux__ && defined __ARM_ARCH

static const char* DecodeArmImplementer( uint32_t v )
{
    static char buf[16];
    switch( v )
    {
    case 0x41: return "ARM";
    case 0x42: return "Broadcom";
    case 0x43: return "Cavium";
    case 0x44: return "DEC";
    case 0x46: return "Fujitsu";
    case 0x48: return "HiSilicon";
    case 0x49: return "Infineon";
    case 0x4d: return "Motorola";
    case 0x4e: return "Nvidia";
    case 0x50: return "Applied Micro";
    case 0x51: return "Qualcomm";
    case 0x53: return "Samsung";
    case 0x54: return "Texas Instruments";
    case 0x56: return "Marvell";
    case 0x61: return "Apple";
    case 0x66: return "Faraday";
    case 0x68: return "HXT";
    case 0x69: return "Intel";
    case 0xc0: return "Ampere Computing";
    default: break;
    }
    sprintf( buf, "0x%x", v );
    return buf;
}

static const char* DecodeArmPart( uint32_t impl, uint32_t part )
{
    static char buf[16];
    switch( impl )
    {
    case 0x41:  // ARM
        switch( part )
        {
        case 0x810: return "810";
        case 0x920: return "920";
        case 0x922: return "922";
        case 0x926: return "926";
        case 0x940: return "940";
        case 0x946: return "946";
        case 0x966: return "966";
        case 0xa20: return "1020";
        case 0xa22: return "1022";
        case 0xa26: return "1026";
        case 0xb02: return "11 MPCore";
        case 0xb36: return "1136";
        case 0xb56: return "1156";
        case 0xb76: return "1176";
        case 0xc05: return " Cortex-A5";
        case 0xc07: return " Cortex-A7";
        case 0xc08: return " Cortex-A8";
        case 0xc09: return " Cortex-A9";
        case 0xc0c: return " Cortex-A12";
        case 0xc0d: return " Rockchip RK3288";
        case 0xc0e: return " Cortex-A17";
        case 0xc0f: return " Cortex-A15";
        case 0xc14: return " Cortex-R4";
        case 0xc15: return " Cortex-R5";
        case 0xc17: return " Cortex-R7";
        case 0xc18: return " Cortex-R8";
        case 0xc20: return " Cortex-M0";
        case 0xc21: return " Cortex-M1";
        case 0xc23: return " Cortex-M3";
        case 0xc24: return " Cortex-M4";
        case 0xc27: return " Cortex-M7";
        case 0xc60: return " Cortex-M0+";
        case 0xd00: return " AArch64 simulator";
        case 0xd01: return " Cortex-A32";
        case 0xd02: return " Cortex-A34";
        case 0xd03: return " Cortex-A53";
        case 0xd04: return " Cortex-A35";
        case 0xd05: return " Cortex-A55";
        case 0xd06: return " Cortex-A65";
        case 0xd07: return " Cortex-A57";
        case 0xd08: return " Cortex-A72";
        case 0xd09: return " Cortex-A73";
        case 0xd0a: return " Cortex-A75";
        case 0xd0b: return " Cortex-A76";
        case 0xd0c: return " Neoverse N1";
        case 0xd0d: return " Cortex-A77";
        case 0xd0e: return " Cortex-A76AE";
        case 0xd0f: return " AEMv8";
        case 0xd13: return " Cortex-R52";
        case 0xd20: return " Cortex-M23";
        case 0xd21: return " Cortex-M33";
        case 0xd22: return " Cortex-M55";
        case 0xd40: return " Neoverse V1";
        case 0xd41: return " Cortex-A78";
        case 0xd42: return " Cortex-A78AE";
        case 0xd43: return " Cortex-A65AE";
        case 0xd44: return " Cortex-X1";
        case 0xd47: return " Cortex-A710";
        case 0xd48: return " Cortex-X2";
        case 0xd49: return " Neoverse N2";
        case 0xd4a: return " Neoverse E1";
        case 0xd4b: return " Cortex-A78C";
        case 0xd4c: return " Cortex-X1C";
        default: break;
        }
    case 0x42:  // Broadcom
        switch( part )
        {
        case 0xf: return " Brahma B15";
        case 0x100: return " Brahma B53";
        case 0x516: return " ThunderX2";
        default: break;
        }
    case 0x43:  // Cavium
        switch( part )
        {
        case 0xa0: return " ThunderX";
        case 0xa1: return " ThunderX 88XX";
        case 0xa2: return " ThunderX 81XX";
        case 0xa3: return " ThunderX 83XX";
        case 0xaf: return " ThunderX2 99xx";
        case 0xb0: return " OcteonTX2";
        case 0xb1: return " OcteonTX2 T98";
        case 0xb2: return " OcteonTX2 T96";
        case 0xb3: return " OcteonTX2 F95";
        case 0xb4: return " OcteonTX2 F95N";
        case 0xb5: return " OcteonTX2 F95MM";
        case 0xb6: return " OcteonTX2 F95O";
        case 0xb8: return " ThunderX3 T110";
        default: break;
        }
    case 0x44:  // DEC
        switch( part )
        {
        case 0xa10: return " SA110";
        case 0xa11: return " SA1100";
        default: break;
        }
    case 0x46:  // Fujitsu
        switch( part )
        {
        case 0x1: return " A64FX";
        default: break;
        }
    case 0x48:  // HiSilicon
        switch( part )
        {
        case 0xd01: return " TSV100";
        case 0xd40: return " Kirin 980";
        default: break;
        }
    case 0x4e:  // Nvidia
        switch( part )
        {
        case 0x0: return " Denver";
        case 0x3: return " Denver 2";
        case 0x4: return " Carmel";
        default: break;
        }
    case 0x50:  // Applied Micro
        switch( part )
        {
        case 0x0: return " X-Gene";
        default: break;
        }
    case 0x51:  // Qualcomm
        switch( part )
        {
        case 0xf: return " Scorpion";
        case 0x2d: return " Scorpion";
        case 0x4d: return " Krait";
        case 0x6f: return " Krait";
        case 0x200: return " Kryo";
        case 0x201: return " Kryo Silver (Snapdragon 821)";
        case 0x205: return " Kryo Gold";
        case 0x211: return " Kryo Silver (Snapdragon 820)";
        case 0x800: return " Kryo 260 / 280 Gold";
        case 0x801: return " Kryo 260 / 280 Silver";
        case 0x802: return " Kryo 385 Gold";
        case 0x803: return " Kryo 385 Silver";
        case 0x804: return " Kryo 485 Gold";
        case 0x805: return " Kryo 4xx/5xx Silver";
        case 0xc00: return " Falkor";
        case 0xc01: return " Saphira";
        default: break;
        }
    case 0x53:  // Samsung
        switch( part )
        {
        case 0x1: return " Exynos M1/M2";
        case 0x2: return " Exynos M3";
        case 0x3: return " Exynos M4";
        case 0x4: return " Exynos M5";
        default: break;
        }
    case 0x54:  // Texas Instruments
        switch( part )
        {
        case 0x925: return " TI925";
        default: break;
        }
    case 0x56:  // Marvell
        switch( part )
        {
        case 0x131: return " Feroceon 88FR131";
        case 0x581: return " PJ4 / PJ4B";
        case 0x584: return " PJ4B-MP / PJ4C";
        default: break;
        }
    case 0x61:  // Apple
        switch( part )
        {
        case 0x1: return " Cyclone";
        case 0x2: return " Typhoon";
        case 0x3: return " Typhoon/Capri";
        case 0x4: return " Twister";
        case 0x5: return " Twister/Elba/Malta";
        case 0x6: return " Hurricane";
        case 0x7: return " Hurricane/Myst";
        case 0x22: return " M1 Icestorm";
        case 0x23: return " M1 Firestorm";
        case 0x24: return " M1 Icestorm Pro";
        case 0x25: return " M1 Firestorm Pro";
        case 0x28: return " M1 Icestorm Max";
        case 0x29: return " M1 Firestorm Max";
        default: break;
        }
    case 0x66:  // Faraday
        switch( part )
        {
        case 0x526: return " FA526";
        case 0x626: return " FA626";
        default: break;
        }
    case 0x68:  // HXT
        switch( part )
        {
        case 0x0: return " Phecda";
        default: break;
        }
    case 0xc0:  // Ampere Computing
        switch( part )
        {
        case 0xac3: return " Ampere1";
        default: break;
        }
    default: break;
    }
    sprintf( buf, " 0x%x", part );
    return buf;
}

#elif defined __APPLE__ && TARGET_OS_IPHONE == 1

static const char* DecodeIosDevice( const char* id )
{
    static const char* DeviceTable[] = {
        "i386", "32-bit simulator",
        "x86_64", "64-bit simulator",
        "iPhone1,1", "iPhone",
        "iPhone1,2", "iPhone 3G",
        "iPhone2,1", "iPhone 3GS",
        "iPhone3,1", "iPhone 4 (GSM)",
        "iPhone3,2", "iPhone 4 (GSM)",
        "iPhone3,3", "iPhone 4 (CDMA)",
        "iPhone4,1", "iPhone 4S",
        "iPhone5,1", "iPhone 5 (A1428)",
        "iPhone5,2", "iPhone 5 (A1429)",
        "iPhone5,3", "iPhone 5c (A1456/A1532)",
        "iPhone5,4", "iPhone 5c (A1507/A1516/1526/A1529)",
        "iPhone6,1", "iPhone 5s (A1433/A1533)",
        "iPhone6,2", "iPhone 5s (A1457/A1518/A1528/A1530)",
        "iPhone7,1", "iPhone 6 Plus",
        "iPhone7,2", "iPhone 6",
        "iPhone8,1", "iPhone 6S",
        "iPhone8,2", "iPhone 6S Plus",
        "iPhone8,4", "iPhone SE",
        "iPhone9,1", "iPhone 7 (CDMA)",
        "iPhone9,2", "iPhone 7 Plus (CDMA)",
        "iPhone9,3", "iPhone 7 (GSM)",
        "iPhone9,4", "iPhone 7 Plus (GSM)",
        "iPhone10,1", "iPhone 8 (CDMA)",
        "iPhone10,2", "iPhone 8 Plus (CDMA)",
        "iPhone10,3", "iPhone X (CDMA)",
        "iPhone10,4", "iPhone 8 (GSM)",
        "iPhone10,5", "iPhone 8 Plus (GSM)",
        "iPhone10,6", "iPhone X (GSM)",
        "iPhone11,2", "iPhone XS",
        "iPhone11,4", "iPhone XS Max",
        "iPhone11,6", "iPhone XS Max China",
        "iPhone11,8", "iPhone XR",
        "iPhone12,1", "iPhone 11",
        "iPhone12,3", "iPhone 11 Pro",
        "iPhone12,5", "iPhone 11 Pro Max",
        "iPhone12,8", "iPhone SE 2nd Gen",
        "iPhone13,1", "iPhone 12 Mini",
        "iPhone13,2", "iPhone 12",
        "iPhone13,3", "iPhone 12 Pro",
        "iPhone13,4", "iPhone 12 Pro Max",
        "iPhone14,2", "iPhone 13 Pro",
        "iPhone14,3", "iPhone 13 Pro Max",
        "iPhone14,4", "iPhone 13 Mini",
        "iPhone14,5", "iPhone 13",
        "iPhone14,6", "iPhone SE 3rd Gen",
        "iPad1,1", "iPad (A1219/A1337)",
        "iPad2,1", "iPad 2 (A1395)",
        "iPad2,2", "iPad 2 (A1396)",
        "iPad2,3", "iPad 2 (A1397)",
        "iPad2,4", "iPad 2 (A1395)",
        "iPad2,5", "iPad Mini (A1432)",
        "iPad2,6", "iPad Mini (A1454)",
        "iPad2,7", "iPad Mini (A1455)",
        "iPad3,1", "iPad 3 (A1416)",
        "iPad3,2", "iPad 3 (A1403)",
        "iPad3,3", "iPad 3 (A1430)",
        "iPad3,4", "iPad 4 (A1458)",
        "iPad3,5", "iPad 4 (A1459)",
        "iPad3,6", "iPad 4 (A1460)",
        "iPad4,1", "iPad Air (A1474)",
        "iPad4,2", "iPad Air (A1475)",
        "iPad4,3", "iPad Air (A1476)",
        "iPad4,4", "iPad Mini 2 (A1489)",
        "iPad4,5", "iPad Mini 2 (A1490)",
        "iPad4,6", "iPad Mini 2 (A1491)",
        "iPad4,7", "iPad Mini 3 (A1599)",
        "iPad4,8", "iPad Mini 3 (A1600)",
        "iPad4,9", "iPad Mini 3 (A1601)",
        "iPad5,1", "iPad Mini 4 (A1538)",
        "iPad5,2", "iPad Mini 4 (A1550)",
        "iPad5,3", "iPad Air 2 (A1566)",
        "iPad5,4", "iPad Air 2 (A1567)",
        "iPad6,3", "iPad Pro 9.7\" (A1673)",
        "iPad6,4", "iPad Pro 9.7\" (A1674)",
        "iPad6,5", "iPad Pro 9.7\" (A1675)",
        "iPad6,7", "iPad Pro 12.9\" (A1584)",
        "iPad6,8", "iPad Pro 12.9\" (A1652)",
        "iPad6,11", "iPad 5th gen (A1822)",
        "iPad6,12", "iPad 5th gen (A1823)",
        "iPad7,1", "iPad Pro 12.9\" 2nd gen (A1670)",
        "iPad7,2", "iPad Pro 12.9\" 2nd gen (A1671/A1821)",
        "iPad7,3", "iPad Pro 10.5\" (A1701)",
        "iPad7,4", "iPad Pro 10.5\" (A1709)",
        "iPad7,5", "iPad 6th gen (A1893)",
        "iPad7,6", "iPad 6th gen (A1954)",
        "iPad7,11", "iPad 7th gen 10.2\" (Wifi)",
        "iPad7,12", "iPad 7th gen 10.2\" (Wifi+Cellular)",
        "iPad8,1", "iPad Pro 11\" (A1980)",
        "iPad8,2", "iPad Pro 11\" (A1980)",
        "iPad8,3", "iPad Pro 11\" (A1934/A1979/A2013)",
        "iPad8,4", "iPad Pro 11\" (A1934/A1979/A2013)",
        "iPad8,5", "iPad Pro 12.9\" 3rd gen (A1876)",
        "iPad8,6", "iPad Pro 12.9\" 3rd gen (A1876)",
        "iPad8,7", "iPad Pro 12.9\" 3rd gen (A1895/A1983/A2014)",
        "iPad8,8", "iPad Pro 12.9\" 3rd gen (A1895/A1983/A2014)",
        "iPad8,9", "iPad Pro 11\" 2nd gen (Wifi)",
        "iPad8,10", "iPad Pro 11\" 2nd gen (Wifi+Cellular)",
        "iPad8,11", "iPad Pro 12.9\" 4th gen (Wifi)",
        "iPad8,12", "iPad Pro 12.9\" 4th gen (Wifi+Cellular)",
        "iPad11,1", "iPad Mini 5th gen (A2133)",
        "iPad11,2", "iPad Mini 5th gen (A2124/A2125/A2126)",
        "iPad11,3", "iPad Air 3rd gen (A2152)",
        "iPad11,4", "iPad Air 3rd gen (A2123/A2153/A2154)",
        "iPad11,6", "iPad 8th gen (WiFi)",
        "iPad11,7", "iPad 8th gen (WiFi+Cellular)",
        "iPad13,1", "iPad Air 4th gen (WiFi)",
        "iPad13,2", "iPad Air 4th gen (WiFi+Cellular)",
        "iPad13,4", "iPad Pro 11\" 3rd gen",
        "iPad13,5", "iPad Pro 11\" 3rd gen",
        "iPad13,6", "iPad Pro 11\" 3rd gen",
        "iPad13,7", "iPad Pro 11\" 3rd gen",
        "iPad13,8", "iPad Pro 12.9\" 5th gen",
        "iPad13,9", "iPad Pro 12.9\" 5th gen",
        "iPad13,10", "iPad Pro 12.9\" 5th gen",
        "iPad13,11", "iPad Pro 12.9\" 5th gen",
        "iPad13,16", "iPad Air 5th Gen (WiFi)",
        "iPad13,17", "iPad Air 5th Gen (WiFi+Cellular)",
        "iPod1,1", "iPod Touch",
        "iPod2,1", "iPod Touch 2nd gen",
        "iPod3,1", "iPod Touch 3rd gen",
        "iPod4,1", "iPod Touch 4th gen",
        "iPod5,1", "iPod Touch 5th gen",
        "iPod7,1", "iPod Touch 6th gen",
        "iPod9,1", "iPod Touch 7th gen",
        nullptr
    };

    auto ptr = DeviceTable;
    while( *ptr )
    {
        if( strcmp( ptr[0], id ) == 0 ) return ptr[1];
        ptr += 2;
    }
    return id;
}

#endif

}
