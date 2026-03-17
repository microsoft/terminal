#include "pch.h"
#include "PtyServer.h"

NTSTATUS PtyServer::handleUserL3GetConsoleMouseInfo()
{
    m_req.u.consoleMsgL3.GetConsoleMouseInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleFontSize()
{
    m_req.u.consoleMsgL3.GetConsoleFontSize;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleCurrentFont()
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3SetConsoleDisplayMode()
{
    m_req.u.consoleMsgL3.SetConsoleDisplayMode;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleDisplayMode()
{
    m_req.u.consoleMsgL3.GetConsoleDisplayMode;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3AddConsoleAlias()
{
    m_req.u.consoleMsgL3.AddConsoleAlias;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleAlias()
{
    m_req.u.consoleMsgL3.GetConsoleAlias;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleAliasesLength()
{
    m_req.u.consoleMsgL3.GetConsoleAliasesLength;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleAliasExesLength()
{
    m_req.u.consoleMsgL3.GetConsoleAliasExesLength;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleAliases()
{
    m_req.u.consoleMsgL3.GetConsoleAliases;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleAliasExes()
{
    m_req.u.consoleMsgL3.GetConsoleAliasExes;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3ExpungeConsoleCommandHistory()
{
    m_req.u.consoleMsgL3.ExpungeConsoleCommandHistory;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3SetConsoleNumberOfCommands()
{
    m_req.u.consoleMsgL3.SetConsoleNumberOfCommands;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleCommandHistoryLength()
{
    m_req.u.consoleMsgL3.GetConsoleCommandHistoryLength;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleCommandHistory()
{
    m_req.u.consoleMsgL3.GetConsoleCommandHistory;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleWindow()
{
    m_req.u.consoleMsgL3.GetConsoleWindow;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleSelectionInfo()
{
    m_req.u.consoleMsgL3.GetConsoleSelectionInfo;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleProcessList()
{
    m_req.u.consoleMsgL3.GetConsoleProcessList;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3GetConsoleHistory()
{
    m_req.u.consoleMsgL3.GetConsoleHistory;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3SetConsoleHistory()
{
    m_req.u.consoleMsgL3.SetConsoleHistory;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS PtyServer::handleUserL3SetConsoleCurrentFont()
{
    return STATUS_NOT_IMPLEMENTED;
}
