

/* this ALWAYS GENERATED file contains the RPC server stubs */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Mon Jan 18 21:14:07 2038
 */
/* Compiler settings for ..\hello.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0622 
    protocol : all , ms_ext, app_config, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)


#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/

#include <string.h>
#include "hello_h.h"

#define TYPE_FORMAT_STRING_SIZE   7                                 
#define PROC_FORMAT_STRING_SIZE   91                                
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _hello_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } hello_MIDL_TYPE_FORMAT_STRING;

typedef struct _hello_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } hello_MIDL_PROC_FORMAT_STRING;

typedef struct _hello_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } hello_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

static const RPC_SYNTAX_IDENTIFIER  _NDR64_RpcTransferSyntax = 
{{0x71710533,0xbeba,0x4937,{0x83,0x19,0xb5,0xdb,0xef,0x9c,0xcc,0x36}},{1,0}};


extern const hello_MIDL_TYPE_FORMAT_STRING hello__MIDL_TypeFormatString;
extern const hello_MIDL_PROC_FORMAT_STRING hello__MIDL_ProcFormatString;
extern const hello_MIDL_EXPR_FORMAT_STRING hello__MIDL_ExprFormatString;

/* Standard interface: hello, ver. 1.0,
   GUID={0x7a98c250,0x6808,0x11cf,{0xb7,0x3b,0x00,0xaa,0x00,0xb6,0x77,0xa7}} */


extern const MIDL_SERVER_INFO hello_ServerInfo;

extern const RPC_DISPATCH_TABLE hello_v1_0_DispatchTable;

static const RPC_SERVER_INTERFACE hello___RpcServerInterface =
    {
    sizeof(RPC_SERVER_INTERFACE),
    {{0x7a98c250,0x6808,0x11cf,{0xb7,0x3b,0x00,0xaa,0x00,0xb6,0x77,0xa7}},{1,0}},
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    (RPC_DISPATCH_TABLE*)&hello_v1_0_DispatchTable,
    0,
    0,
    0,
    &hello_ServerInfo,
    0x06000000
    };
RPC_IF_HANDLE hello_v1_0_s_ifspec = (RPC_IF_HANDLE)& hello___RpcServerInterface;

extern const MIDL_STUB_DESC hello_StubDesc;


#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const hello_MIDL_PROC_FORMAT_STRING hello__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure GetDoCount */

			0x32,		/* FC_BIND_PRIMITIVE */
			0x48,		/* Old Flags:  */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 16 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Return value */

/* 26 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 28 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure HelloProc */

/* 32 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x48,		/* Old Flags:  */
/* 34 */	NdrFcLong( 0x0 ),	/* 0 */
/* 38 */	NdrFcShort( 0x1 ),	/* 1 */
/* 40 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 42 */	NdrFcShort( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0x0 ),	/* 0 */
/* 46 */	0x42,		/* Oi2 Flags:  clt must size, has ext, */
			0x1,		/* 1 */
/* 48 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 50 */	NdrFcShort( 0x0 ),	/* 0 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x0 ),	/* 0 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter pszString */

/* 58 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 60 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 62 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Procedure Shutdown */

/* 64 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x48,		/* Old Flags:  */
/* 66 */	NdrFcLong( 0x0 ),	/* 0 */
/* 70 */	NdrFcShort( 0x2 ),	/* 2 */
/* 72 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 74 */	NdrFcShort( 0x0 ),	/* 0 */
/* 76 */	NdrFcShort( 0x0 ),	/* 0 */
/* 78 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 80 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 82 */	NdrFcShort( 0x0 ),	/* 0 */
/* 84 */	NdrFcShort( 0x0 ),	/* 0 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */

			0x0
        }
    };

static const hello_MIDL_TYPE_FORMAT_STRING hello__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */

			0x0
        }
    };

static const unsigned short hello_FormatStringOffsetTable[] =
    {
    0,
    32,
    64
    };


static const RPC_DISPATCH_FUNCTION hello_table[] =
    {
    NdrServerCall2,
    NdrServerCall2,
    NdrServerCall2,
    0
    };
static const RPC_DISPATCH_TABLE hello_v1_0_DispatchTable = 
    {
    3,
    (RPC_DISPATCH_FUNCTION*)hello_table
    };


#endif /* defined(_M_AMD64)*/



/* this ALWAYS GENERATED file contains the RPC server stubs */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Mon Jan 18 21:14:07 2038
 */
/* Compiler settings for ..\hello.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0622 
    protocol : all , ms_ext, app_config, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)




#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif


#include "ndr64types.h"
#include "pshpack8.h"


typedef 
struct 
{
    struct _NDR64_PROC_FORMAT frag1;
}
__midl_frag7_t;
extern const __midl_frag7_t __midl_frag7;

typedef 
struct _NDR64_CONFORMANT_STRING_FORMAT
__midl_frag6_t;
extern const __midl_frag6_t __midl_frag6;

typedef 
struct _NDR64_POINTER_FORMAT
__midl_frag5_t;
extern const __midl_frag5_t __midl_frag5;

typedef 
struct 
{
    struct _NDR64_PROC_FORMAT frag1;
    struct _NDR64_PARAM_FORMAT frag2;
}
__midl_frag4_t;
extern const __midl_frag4_t __midl_frag4;

typedef 
NDR64_FORMAT_CHAR
__midl_frag3_t;
extern const __midl_frag3_t __midl_frag3;

typedef 
struct 
{
    struct _NDR64_PROC_FORMAT frag1;
    struct _NDR64_PARAM_FORMAT frag2;
}
__midl_frag2_t;
extern const __midl_frag2_t __midl_frag2;

typedef 
NDR64_FORMAT_UINT32
__midl_frag1_t;
extern const __midl_frag1_t __midl_frag1;

static const __midl_frag7_t __midl_frag7 =
{ 
/* Shutdown */
    { 
    /* Shutdown */      /* procedure Shutdown */
        (NDR64_UINT32) 66 /* 0x42 */,    /* primitive handle */ /* IsIntrepreted */
        (NDR64_UINT32) 0 /* 0x0 */ ,  /* Stack size */
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */
    }
};

static const __midl_frag6_t __midl_frag6 =
{ 
/* *wchar_t */
    { 
    /* *wchar_t */
        0x64,    /* FC64_CONF_WCHAR_STRING */
        { 
        /* *wchar_t */
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        },
        (NDR64_UINT16) 2 /* 0x2 */
    }
};

static const __midl_frag5_t __midl_frag5 =
{ 
/* *wchar_t */
    0x20,    /* FC64_RP */
    (NDR64_UINT8) 0 /* 0x0 */,
    (NDR64_UINT16) 0 /* 0x0 */,
    &__midl_frag6
};

static const __midl_frag4_t __midl_frag4 =
{ 
/* HelloProc */
    { 
    /* HelloProc */      /* procedure HelloProc */
        (NDR64_UINT32) 262210 /* 0x40042 */,    /* primitive handle */ /* IsIntrepreted, ClientMustSize */
        (NDR64_UINT32) 8 /* 0x8 */ ,  /* Stack size */
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 1 /* 0x1 */,
        (NDR64_UINT16) 0 /* 0x0 */
    },
    { 
    /* pszString */      /* parameter pszString */
        &__midl_frag6,
        { 
        /* pszString */
            1,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* MustSize, MustFree, [in], SimpleRef */
        (NDR64_UINT16) 0 /* 0x0 */,
        0 /* 0x0 */,   /* Stack offset */
    }
};

static const __midl_frag3_t __midl_frag3 =
0x5    /* FC64_INT32 */;

static const __midl_frag2_t __midl_frag2 =
{ 
/* GetDoCount */
    { 
    /* GetDoCount */      /* procedure GetDoCount */
        (NDR64_UINT32) 524354 /* 0x80042 */,    /* primitive handle */ /* IsIntrepreted, HasReturn */
        (NDR64_UINT32) 8 /* 0x8 */ ,  /* Stack size */
        (NDR64_UINT32) 0 /* 0x0 */,
        (NDR64_UINT32) 8 /* 0x8 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 0 /* 0x0 */,
        (NDR64_UINT16) 1 /* 0x1 */,
        (NDR64_UINT16) 0 /* 0x0 */
    },
    { 
    /* int */      /* parameter int */
        &__midl_frag3,
        { 
        /* int */
            0,
            0,
            0,
            0,
            1,
            1,
            1,
            1,
            0,
            0,
            0,
            0,
            0,
            (NDR64_UINT16) 0 /* 0x0 */,
            0
        },    /* [out], IsReturn, Basetype, ByValue */
        (NDR64_UINT16) 0 /* 0x0 */,
        0 /* 0x0 */,   /* Stack offset */
    }
};

static const __midl_frag1_t __midl_frag1 =
(NDR64_UINT32) 0 /* 0x0 */;


#include "poppack.h"


static const FormatInfoRef hello_Ndr64ProcTable[] =
    {
    &__midl_frag2,
    &__midl_frag4,
    &__midl_frag7
    };


static const MIDL_STUB_DESC hello_StubDesc = 
    {
    (void *)& hello___RpcServerInterface,
    MIDL_user_allocate,
    MIDL_user_free,
    0,
    0,
    0,
    0,
    0,
    hello__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x60001, /* Ndr library version */
    0,
    0x801026e, /* MIDL Version 8.1.622 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x2000001, /* MIDL flag */
    0, /* cs routines */
    (void *)& hello_ServerInfo,   /* proxy/server info */
    0
    };

static const RPC_DISPATCH_FUNCTION hello_NDR64__table[] =
    {
    NdrServerCallAll,
    NdrServerCallAll,
    NdrServerCallAll,
    0
    };
static const RPC_DISPATCH_TABLE hello_NDR64__v1_0_DispatchTable = 
    {
    3,
    (RPC_DISPATCH_FUNCTION*)hello_NDR64__table
    };

static const MIDL_SYNTAX_INFO hello_SyntaxInfo [  2 ] = 
    {
    {
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    (RPC_DISPATCH_TABLE*)&hello_v1_0_DispatchTable,
    hello__MIDL_ProcFormatString.Format,
    hello_FormatStringOffsetTable,
    hello__MIDL_TypeFormatString.Format,
    0,
    0,
    0
    }
    ,{
    {{0x71710533,0xbeba,0x4937,{0x83,0x19,0xb5,0xdb,0xef,0x9c,0xcc,0x36}},{1,0}},
    (RPC_DISPATCH_TABLE*)&hello_NDR64__v1_0_DispatchTable,
    0 ,
    (unsigned short *) hello_Ndr64ProcTable,
    0,
    0,
    0,
    0
    }
    };


static const SERVER_ROUTINE hello_ServerRoutineTable[] = 
    {
    (SERVER_ROUTINE)GetDoCount,
    (SERVER_ROUTINE)HelloProc,
    (SERVER_ROUTINE)Shutdown
    };

static const MIDL_SERVER_INFO hello_ServerInfo = 
    {
    &hello_StubDesc,
    hello_ServerRoutineTable,
    hello__MIDL_ProcFormatString.Format,
    (unsigned short *) hello_FormatStringOffsetTable,
    0,
    (RPC_SYNTAX_IDENTIFIER*)&_NDR64_RpcTransferSyntax,
    2,
    (MIDL_SYNTAX_INFO*)hello_SyntaxInfo
    };
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_AMD64)*/

