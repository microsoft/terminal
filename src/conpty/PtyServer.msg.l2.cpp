#include "pch.h"
#include "PtyServer.h"

NTSTATUS PtyServer::handleUserL2FillConsoleOutput()
{
    m_req.u.consoleMsgL2.FillConsoleOutput;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2GenerateConsoleCtrlEvent()
{
    m_req.u.consoleMsgL2.GenerateConsoleCtrlEvent;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleActiveScreenBuffer()
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2FlushConsoleInputBuffer()
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleCP()
{
    m_req.u.consoleMsgL2.SetConsoleCP;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2GetConsoleCursorInfo()
{
    m_req.u.consoleMsgL2.GetConsoleCursorInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleCursorInfo()
{
    m_req.u.consoleMsgL2.SetConsoleCursorInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2GetConsoleScreenBufferInfo()
{
    m_req.u.consoleMsgL2.GetConsoleScreenBufferInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleScreenBufferInfo()
{
    m_req.u.consoleMsgL2.SetConsoleScreenBufferInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleScreenBufferSize()
{
    m_req.u.consoleMsgL2.SetConsoleScreenBufferSize;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleCursorPosition()
{
    m_req.u.consoleMsgL2.SetConsoleCursorPosition;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2GetLargestConsoleWindowSize()
{
    m_req.u.consoleMsgL2.GetLargestConsoleWindowSize;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2ScrollConsoleScreenBuffer()
{
    m_req.u.consoleMsgL2.ScrollConsoleScreenBuffer;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleTextAttribute()
{
    m_req.u.consoleMsgL2.SetConsoleTextAttribute;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleWindowInfo()
{
    m_req.u.consoleMsgL2.SetConsoleWindowInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2ReadConsoleOutputString()
{
    m_req.u.consoleMsgL2.ReadConsoleOutputString;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2WriteConsoleInput()
{
    m_req.u.consoleMsgL2.WriteConsoleInput;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2WriteConsoleOutput()
{
    m_req.u.consoleMsgL2.WriteConsoleOutput;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2WriteConsoleOutputString()
{
    m_req.u.consoleMsgL2.WriteConsoleOutputString;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2ReadConsoleOutput()
{
    m_req.u.consoleMsgL2.ReadConsoleOutput;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2GetConsoleTitle()
{
    m_req.u.consoleMsgL2.GetConsoleTitle;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL2SetConsoleTitle()
{
    m_req.u.consoleMsgL2.SetConsoleTitle;
    return STATUS_NOT_IMPLEMENTED;
}
