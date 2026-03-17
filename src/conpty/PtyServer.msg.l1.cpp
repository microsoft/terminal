#include "pch.h"
#include "PtyServer.h"

NTSTATUS PtyServer::handleUserL1GetConsoleCP()
{
    m_req.u.consoleMsgL1.GetConsoleCP;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1GetConsoleMode()
{
    m_req.u.consoleMsgL1.GetConsoleMode;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1SetConsoleMode()
{
    m_req.u.consoleMsgL1.SetConsoleMode;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1GetNumberOfConsoleInputEvents()
{
    m_req.u.consoleMsgL1.GetNumberOfConsoleInputEvents;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1GetConsoleInput()
{
    m_req.u.consoleMsgL1.GetConsoleInput;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1ReadConsole()
{
    m_req.u.consoleMsgL1.ReadConsole;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1WriteConsole()
{
    m_req.u.consoleMsgL1.WriteConsole;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL1GetConsoleLangId()
{
    m_req.u.consoleMsgL1.GetConsoleLangId;
    return STATUS_NOT_IMPLEMENTED;
}
