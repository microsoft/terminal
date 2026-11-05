# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

"""One Copilot orchestrator, scoped translation sub-agents, no shell or file-write access."""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path

from copilot import CopilotClient
from copilot.generated.rpc import PermissionDecisionApproveOnce, PermissionDecisionReject
from copilot.generated.session_events import PermissionRequest, PermissionRequestRead
from copilot.session import PermissionInvocation, PreToolUseHookInput, PreToolUseHookOutput
from copilot.tools import Tool, ToolInvocation, ToolResult

from localize import ROOT, INSTRUCTIONS, batch_prompt
from resw import fail, validate_response
import terminology

LOG = logging.getLogger(__name__)
RESEARCH_TOOLS = ["grep", "glob", "view"]


def check_read_path(path: str, working_directory: str) -> None:
    resolved = (Path(working_directory) / Path(path).expanduser()).resolve()
    if not resolved.is_relative_to(ROOT):
        fail(f"Read access is limited to the Git root {ROOT}: {path}")


def on_pre_tool_use(data: PreToolUseHookInput, invocation: dict[str, str]) -> PreToolUseHookOutput | None:
    if data["toolName"] not in RESEARCH_TOOLS:
        return None
    try:
        args = data["toolArgs"]
        if not isinstance(args, dict):
            fail("Expected tool arguments")
        paths = args["path"] if data["toolName"] == "view" else args.get("paths", [data["workingDirectory"]])
        if isinstance(paths, str):
            paths = [paths]
        if not isinstance(paths, list) or not paths or any(not isinstance(path, str) for path in paths):
            fail("Expected a path or a nonempty list of paths")
        for path in paths:
            check_read_path(path, data["workingDirectory"])
    except (KeyError, OSError, RuntimeError, ValueError) as error:
        LOG.warning("Tool %s rejected: %s", data["toolName"], error)
        return {"permissionDecision": "deny", "permissionDecisionReason": str(error)}
    # Leave approval to the runtime and permission handler, including managed policies.
    return None


def on_permission_request(
    request: PermissionRequest, invocation: PermissionInvocation,
) -> PermissionDecisionApproveOnce | PermissionDecisionReject:
    try:
        if not isinstance(request, PermissionRequestRead):
            fail("Only repository read permissions may be approved")
        if request.managed_approval_required or request.request_sandbox_bypass:
            fail("Managed approval and sandbox bypass cannot be granted automatically")
        check_read_path(request.path, str(ROOT))
        if request.resolved_path is not None:
            check_read_path(request.resolved_path, str(ROOT))
    except (OSError, RuntimeError, ValueError) as error:
        LOG.warning("Permission %s rejected: %s", request.kind, error)
        return PermissionDecisionReject(feedback=str(error))
    return PermissionDecisionApproveOnce()


async def translate(
    batches: dict[str, dict],
    collections: dict[str, terminology.Collection | None],
    model: str | None, timeout: int,
) -> dict[str, dict[str, str]]:
    complete = set()
    translated: dict[str, dict[str, str]] = {}
    if not batches:
        print("No translation batches needed.", flush=True)
        return translated
    instructions = INSTRUCTIONS.read_text(encoding="utf-8")
    owners: dict[str, str] = {}
    event_errors = []

    def claim(invocation: ToolInvocation, name: str) -> dict:
        if name not in batches:
            fail(f"Unknown batch: {name}")
        if not invocation.agent_id:
            fail("Only a translation sub-agent may load or submit a batch; delegate with task")
        if name in owners and owners[name] != invocation.agent_id:
            fail(f"Batch already assigned to another sub-agent: {name}")
        owners[name] = invocation.agent_id
        return batches[name]

    async def dispatch(invocation: ToolInvocation) -> ToolResult:
        args = invocation.arguments
        try:
            if not isinstance(args, dict):
                fail("Expected tool arguments")
            match invocation.tool_name:
                case "load_batch":
                    batch = claim(invocation, args["batch"])
                    result = batch_prompt(batch)
                case "submit_batch":
                    name = args["batch"]
                    batch = claim(invocation, name)
                    if name in complete:
                        fail(f"Batch already completed: {name}")
                    values = validate_response(batch["entries"], batch["languages"], args["translations"])
                    translated.update(values)
                    complete.add(name)
                    count = len(batch["entries"]) * len(batch["languages"])
                    print(f"Validated {name}: {count} translations across {len(batch['languages'])} languages", flush=True)
                    result = f"Saved and validated {count} translations. Batch complete."
                case "lookup_term":
                    batch = claim(invocation, args["batch"])
                    result = "\n\n".join(
                        f"{language}:\n" + (collection.lookup(args["term"]) if collection else
                                           "No Microsoft terminology collection is available for this language.")
                        for language in batch["languages"] for collection in [collections[language]]
                    )
                case "report_warning":
                    batch = claim(invocation, args["batch"])
                    message = args["message"]
                    if not isinstance(message, str) or not message.strip() or len(message) > 2000:
                        fail("Warning must be 1..2000 characters")
                    LOG.warning("%s (%s): %s", ", ".join(batch["languages"]), args["batch"], message)
                    result = "Warning recorded for the commit message."
                case _:
                    fail(f"Unknown tool: {invocation.tool_name}")
            return ToolResult(text_result_for_llm=result)
        except (ValueError, KeyError, TypeError) as error:
            # Invalid model requests are explicit tool errors, so the agent can correct them.
            LOG.warning("Tool %s rejected: %s", invocation.tool_name, error)
            return ToolResult(result_type="failure", text_result_for_llm=str(error), error=str(error))

    def tool(name: str, description: str, properties: dict) -> Tool:
        return Tool(name=name, description=description, handler=dispatch, skip_permission=True, defer="never",
                    parameters={"type": "object", "properties": properties, "required": list(properties),
                                "additionalProperties": False})

    string = {"type": "string"}
    tools = [
        tool("load_batch", "Read one assigned key batch, its target languages, and all terminology candidates.", {"batch": string}),
        tool("submit_batch", "Validate and retain every key in every batch language. Fix rejected values and retry.",
             {"batch": string, "translations": {"type": "array", "items": {
                 "type": "object", "properties": {"id": string, "values": {
                     "type": "object", "additionalProperties": string}},
                 "required": ["id", "values"], "additionalProperties": False}}}),
        tool("lookup_term", "Find all concepts/definitions for an exact English term in every batch language.",
             {"batch": string, "term": string}),
        tool("report_warning", "Record unresolved linguistic concerns for the commit message.",
             {"batch": string, "message": string}),
    ]

    def on_event(event) -> None:
        if event.type.value == "session.error":
            message = getattr(event.data, "message", str(event.data))
            event_errors.append(message)
            LOG.error("Copilot: %s", message)
        elif event.type.value in ("subagent.started", "subagent.completed", "subagent.failed"):
            print(f"{event.type.value}: {event.agent_id or ''}", flush=True)

    token = os.environ.get("COPILOT_GITHUB_TOKEN") or os.environ.get("GITHUB_TOKEN")
    async with CopilotClient(working_directory=str(ROOT), github_token=token) as client:
        session = await client.create_session(
            model=model,
            working_directory=str(ROOT),
            on_permission_request=on_permission_request,
            hooks={"on_pre_tool_use": on_pre_tool_use},
            tools=tools,
            available_tools=["task", "read_agent", "list_agents", "write_agent",
                             *RESEARCH_TOOLS, *[tool.name for tool in tools]],
            custom_agents=[{
                "name": "translator",
                "description": "Research one assigned key batch and translate it into all its languages.",
                "prompt": instructions + "\nUse load_batch once, then submit_batch for your assigned ID. "
                          "Do not delegate. Research each key once and reuse that context across languages. "
                          "Submit only after resolving terminology and UI label references. "
                          "Return a short completion summary, not the translations.",
                "tools": [*RESEARCH_TOOLS, *[tool.name for tool in tools]],
            }],
            custom_agents_local_only=True,
            coauthor_enabled=False,
            enable_file_hooks=False,
            enable_host_git_operations=False,
            enable_config_discovery=False,
            enable_skills=False,
            skip_custom_instructions=True,
            large_output={"enabled": False},
            on_event=on_event,
        )
        try:
            inventory = [
                {"batch": name, "languages": batch["languages"], "entries": len(batch["entries"])}
                for name, batch in batches.items()
            ]
            prompt = (
                "You coordinate Windows Terminal localization. Do not translate strings yourself. "
                "Use task to delegate every batch below to a fresh translator sub-agent. "
                "Give each agent exactly one batch ID; it must use load_batch and submit_batch. "
                "Each batch owns its keys in every listed language; never split it by language. "
                "Keep at most four agents running concurrently. Wait for all agents to finish. "
                "Do not delegate the same batch twice. The tools validate and retain responses in memory. "
                "If a batch cannot be completed, report the failure; never invent a successful result. "
                "Only submit_batch can accept translations. Do not invoke shell commands or publish changes.\n"
                + json.dumps(inventory)
            )
            await session.send_and_wait(prompt, timeout=timeout)
        finally:
            # Detach before deletion so the runtime stops sending events for this session.
            try:
                await session.disconnect()
            finally:
                await client.delete_session(session.session_id)
    if event_errors:
        fail(f"Copilot reported errors: {event_errors}")
    if missing := batches.keys() - complete:
        fail(f"Missing translation batches: {sorted(missing)}")
    print(f"All {len(batches)} batches completed and validated.", flush=True)
    return translated
