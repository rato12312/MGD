---
name: opencode
description: Use when working with OpenCode plugins, SDK, configuration, or APIs.
---

# OpenCode Navigation

## Overview

Entry point for all OpenCode-related work. Routes to specialized skills based on the specific domain: plugin development, SDK usage, or configuration.

**Core principle**: Analyze the question to identify which OpenCode domains are involved, then load the corresponding skills.

## When to Use

Use this skill when the user's question involves:
- OpenCode plugins or plugin development
- @opencode-ai/sdk or programmatic control
- opencode.json configuration
- OpenCode APIs or interfaces
- Any combination of the above

## Routing Logic

**Step 1: Analyze the question**

Identify two aspects:

**A) Is this exploratory or specific?**

Exploratory (needs brainstorming):
- "I want to create/build/add..." without clear specifications
- "Help me design..." or "How should I implement..."
- Vague requests lacking details
- Involves design decisions or architecture planning

Specific (direct technical question):
- "How do I [specific API/method]?"
- "What's the syntax for...?"
- "Why does [specific code] fail?"
- "Show me an example of [specific feature]"

**B) Which technical domains are involved?**

Plugin Development:
- Mentions `@opencode-ai/plugin`
- Creating/debugging plugins
- Custom tools, hooks, agents
- Plugin lifecycle or registration
- `.opencode/plugin/` directory

SDK:
- Mentions `@opencode-ai/sdk`
- Programmatic control of OpenCode
- Session/message/event APIs
- Client-server communication
- SDK types or methods

Configuration:
- Mentions `opencode.json` or `opencode.jsonc`
- Provider/model configuration
- MCP servers, LSP servers
- Permissions, commands
- Agent configuration

**Step 2: Load all relevant skills together**

Based on your analysis, load the appropriate combination:

| Request Type | Technical Domains | Skills to Load |
|--------------|-------------------|----------------|
| Exploratory | Plugin only | `cclover/brainstorming`, `opencode-plugin-development` |
| Exploratory | SDK only | `cclover/brainstorming`, `opencode-sdk` |
| Exploratory | Configuration only | `cclover/brainstorming`, `opencode-configuration` |
| Exploratory | Plugin + SDK | `cclover/brainstorming`, `opencode-plugin-development`, `opencode-sdk` |
| Exploratory | Plugin + Config | `cclover/brainstorming`, `opencode-plugin-development`, `opencode-configuration` |
| Exploratory | SDK + Config | `cclover/brainstorming`, `opencode-sdk`, `opencode-configuration` |
| Exploratory | All three | `cclover/brainstorming`, `opencode-plugin-development`, `opencode-sdk`, `opencode-configuration` |
| Specific | Plugin only | `opencode-plugin-development` |
| Specific | SDK only | `opencode-sdk` |
| Specific | Configuration only | `opencode-configuration` |
| Specific | Plugin + SDK | `opencode-plugin-development`, `opencode-sdk` |
| Specific | Plugin + Config | `opencode-plugin-development`, `opencode-configuration` |
| Specific | SDK + Config | `opencode-sdk`, `opencode-configuration` |
| Specific | All three | `opencode-plugin-development`, `opencode-sdk`, `opencode-configuration` |

**Key principle**: Load ALL relevant skills in ONE message. Don't load brainstorming alone and stop.

**Step 3: Proceed with loaded skills**

After loading the appropriate skills, use them to answer the user's question with specific, accurate information.

## Examples

**Example 0: Exploratory plugin request**
- Question: "I want to create an OpenCode plugin to help with task management"
- Analysis: Exploratory (lacks specifications) + Plugin domain
- Action: Load `cclover/brainstorming` AND `opencode-plugin-development` together

**Example 1: Plugin only**
- Question: "How do I create a custom tool in my OpenCode plugin?"
- Analysis: Plugin development only
- Action: Load `opencode-plugin-development`

**Example 2: SDK only**
- Question: "How do I use @opencode-ai/sdk to create a session?"
- Analysis: SDK usage only
- Action: Load `opencode-sdk`

**Example 3: Configuration only**
- Question: "How do I configure MCP servers in opencode.json?"
- Analysis: Configuration only
- Action: Load `opencode-configuration`

**Example 4: Plugin + Configuration**
- Question: "I wrote a plugin with a custom tool. How do I configure its permissions in opencode.json?"
- Analysis: Both plugin development and configuration
- Action: Load `opencode-plugin-development`, `opencode-configuration`

**Example 5: SDK + Configuration**
- Question: "My @opencode-ai/sdk connection fails because the provider config in opencode.json is wrong"
- Analysis: Both SDK and configuration
- Action: Load `opencode-sdk`, `opencode-configuration`

## Common Mistakes

| Mistake | Why It's Wrong | How to Avoid |
|---------|----------------|--------------|
| Loading brainstorming alone | Stops after loading brainstorming without technical skills | Load brainstorming AND technical skills together |
| Skipping brainstorming check | Jumps to implementation without clarifying vague requests | Always check Step 0 first |
| Missing a domain | Incomplete answer | Check for all three domains systematically |
| Guessing without analysis | May load wrong skills | Always analyze the question first |
| Not loading multiple skills | Missing cross-domain knowledge | Load all relevant domains |

## Red Flags - Check Yourself

If you're thinking:
- "User request is exploratory, I'll load brainstorming first" → Load brainstorming AND technical skills together
- "User said 'I want to create...' so I'll just implement" → Check if brainstorming needed first
- "I'll just load one skill" → Did you check for multiple domains?
- "This seems like configuration" → Did you check for plugin/SDK mentions?
- "I'll answer without loading skills" → You're missing specialized knowledge
- "I'll load all three just in case" → Analyze first, don't waste context

**All of these mean: Stop and analyze the question systematically.**
