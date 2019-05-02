//////////////////////////////////////////////////////////////////////
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
//  StructureArray.h
//
//          CVoidStructureArray declaration.
//          CStructureArray declaration.
//
//////////////////////////////////////////////////////////////////////

#pragma once

class CVoidStructureArray
{
public:
    CVoidStructureArray(int iElementSize, int iInitSize = 0) 
    {
        ULONG ulInitSize;
        ULONG ulElementSize;
        ULONG ulBufferSize;

        _cElements      = 0;
        _pData          = NULL;
        _iAllocatedSize = 0;

        _iElementSize   = iElementSize;

        if (iInitSize)
        {
            if (IntToULong(iInitSize, &ulInitSize) != S_OK) {
                return;
            }
            if (IntToULong(_iElementSize, &ulElementSize) != S_OK) {
                return;
            }
            if (ULongMult(ulInitSize, ulElementSize, &ulBufferSize) != S_OK) {
                return;
            }

            _pData = (BYTE *)LocalAlloc(LPTR, ulBufferSize);
            if (_pData)
            {
                _iAllocatedSize = iInitSize;
            }
        }
    }
    virtual ~CVoidStructureArray() { LocalFree(_pData); }

    //
    // CVoidStructureArray::GetAt() won't return invalid address or NULL address.
    // This function should used to the same idea of element reference as i = a[...]
    //
    // Note that,
    // CVoidStructureArray::GetAt() function should not _pData NULL check and iIndex validation.
    // So caller should check empty element and out of index.
    //
    inline void* GetAt(int iIndex) const
    {
        FAIL_FAST_IF(!(iIndex >= 0));
        FAIL_FAST_IF(!(iIndex <= _cElements)); // there's code that uses the first invalid offset for loop termination
        FAIL_FAST_IF(!(_pData != NULL));

        return _pData + (iIndex * _iElementSize);
    }

    BOOL InsertAt(int iIndex, int cElements = 1)
    {
        BYTE *pb;
        int iSizeNew;

        // check integer overflow.
        if ((iIndex < 0) ||
            (cElements < 0) ||
            (iIndex > _cElements) ||
            (_cElements > _cElements + cElements))
        {
            return FALSE;
        }

        // allocate space if necessary
        if (_iAllocatedSize < _cElements + cElements)
        {
            // allocate 1.5x what we need to avoid future allocs
            iSizeNew = max(_cElements + cElements, _cElements + _cElements / 2);

            UINT uiAllocSize;
            if (FAILED(UIntMult(iSizeNew, _iElementSize, &uiAllocSize)) || ((int)uiAllocSize) < 0)
            {
                return FALSE;
            }

            if ((pb = (_pData == NULL) ? (BYTE *)LocalAlloc(LPTR, uiAllocSize) :
                                         (BYTE *)LocalReAlloc(_pData, uiAllocSize, LMEM_MOVEABLE | LMEM_ZEROINIT))
                == NULL)
            {
                return FALSE;
            }

            _pData          = pb;
            _iAllocatedSize = iSizeNew;
        }

        if (iIndex < _cElements)
        {
            // make room for the new addition
            memmove(ElementPointer(iIndex + cElements),
                    ElementPointer(iIndex),
                    (_cElements - iIndex)*_iElementSize);
        }

        _cElements += cElements;
        FAIL_FAST_IF(!(_iAllocatedSize >= _cElements));

        return TRUE;
    }

    void RemoveAt(int iIndex, int cElements)
    {
        int iSizeNew;

        // check integer overflow.
        if ((iIndex < 0) ||
            (cElements < 0) ||
            (iIndex > _cElements) ||
            (_cElements > _cElements + cElements))
        {
            return;
        }

        if (iIndex + cElements < _cElements)
        {
            // shift following eles left
            memmove(ElementPointer(iIndex),
                    ElementPointer(iIndex + cElements),
                    (_cElements - iIndex - cElements)*_iElementSize);
        }

        _cElements -= cElements;

        // free mem when array contents uses less than half alloc'd mem
        iSizeNew = _iAllocatedSize / 2;
        if (iSizeNew > _cElements)
        {
            CompactSize(iSizeNew);
        }
    }

    int Count() const { return _cElements; }

    void *Append(int cElements = 1)
    {
        return InsertAt(Count(), cElements) ? GetAt(Count()-cElements) : NULL;
    }

    void Clear()
    {
        LocalFree(_pData);
        _pData = NULL;
        _cElements = _iAllocatedSize = 0;
    }

private:
    void CompactSize(int iSizeNew)
    {
        BYTE *pb;
        ULONG ulSizeNew;
        ULONG ulElementSize;
        ULONG ulBufferSize;

        FAIL_FAST_IF(!(iSizeNew <= _iAllocatedSize));
        FAIL_FAST_IF(!(_cElements <= iSizeNew));

        if (iSizeNew == _iAllocatedSize)   // LocalReAlloc will actually re-alloc!  Don't let it.
            return;

        if (IntToULong(iSizeNew, &ulSizeNew) != S_OK) {
            return;
        }
        if (IntToULong(_iElementSize, &ulElementSize) != S_OK) {
            return;
        }
        if (ULongMult(ulSizeNew, ulElementSize, &ulBufferSize) != S_OK) {
            return;
        }
        if ((pb = (BYTE *)LocalReAlloc(_pData, ulBufferSize, LMEM_MOVEABLE | LMEM_ZEROINIT)) != NULL)
        {
            _pData          = pb;
            _iAllocatedSize = iSizeNew;
        }
    }

    BYTE *ElementPointer(int iIndex) const
    {
        return _pData + (iIndex * _iElementSize);
    }

private:
    BYTE  *_pData;             // the actual array of data
    int    _cElements;         // number of elements in the array
    int    _iAllocatedSize;    // maximum allocated size (in void *'s) of the array

    int    _iElementSize;      // size of one element
};

//////////////////////////////////////////////////////////////////////
//
//          CStructureArray declaration.
//
//////////////////////////////////////////////////////////////////////

//
// typesafe version
//
template<class T>
class CStructureArray : public CVoidStructureArray
{
public:
    CStructureArray(int nInitSize = 0):CVoidStructureArray(sizeof(T), nInitSize) {}

    T *GetAt(int iIndex) { return (T *)CVoidStructureArray::GetAt(iIndex); }

    T *Append(int cElements = 1)
    {
        T *ret;
        ret = (T *)CVoidStructureArray::Append(cElements);
        return ret;
    }
};
