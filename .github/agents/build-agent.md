# Build Agent

## Purpose

The Build Agent is responsible for:
- Rebuilding the entire C++/DirectX Render Engine project after changes.
- Detecting all build errors and warnings in the output.
- Reporting them to the team and generating a checklist for automatic or manual fixes.
- Optionally, coordinating with other agents (e.g., Snippet Agent or Fix Agent) to apply code corrections.

## Responsibilities

- Run the project build process using the preferred compiler (`cl.exe`, `msbuild`, CMake, etc.).
- Parse build output for errors and warnings.
- Record and categorize issues by file, line, and type (error, warning).
- Add actionable items (checklist) for each issue found.
- Communicate with Fix Agent to attempt automated fixes, if approved.

## Tasks

- [ ] Rebuild the solution after each code change or at every user prompt in the VS Code chat.
- [ ] Generate a full error/warning report after each build.
- [ ] Update the issue/fix list as errors are resolved.
- [ ] Notify the team/triggered agent when all issues are fixed.

## Interfaces (Communication)

- **Input:** Triggered after each chat prompt by the developer.
- **Output:** Updates status checklist, error/warning report files, and notifies relevant subagents (e.g., Fix Agent).
- **Dependencies:** None; initiates build independently, but communicates results to other agents.

## Rules

- Must never ignore errors or warnings; report all findings.
- Prioritize errors over warnings.
- Only mark tasks as complete once verified by a successful build and clean output.
- Logs all actions and communication in a dedicated `buildAgent.log`.

## Triggering

- **Trigger:** Immediately after every prompt/command the developer enters in the VS Code chat, the Build Agent should:
  1. Rebuild the project.
  2. Report all current errors and warnings.
  3. Hand off unresolved issues to the Fix Agent or raise them for manual attention.

### Automated Workflow

- Integrate with a VS Code extension or bot that listens for user chat prompts.
- After user submits a prompt, automatically:
  1. Invoke the build pipeline.
  2. Channel the output to `buildAgentOutput.md` and/or chat.
  3. Update the checklist of build issues for immediate attention.


## Example Workflow

1. **User Prompt:** Developer enters a new command or makes a code modification.
2. **Automatic Build:** Build Agent detects prompt and initiates a full rebuild.
3. **Analysis:** It collects build errors/warnings and produces a detailed checklist.
4. **Hand-off/Fixes:** Passes each item to manual review or to another fixing agent.
5. **Confirmation:** Once the agent confirms a clean build, it logs completion.

---

## Sample Issue Checklist for a Build

- [ ] `src/renderer.cpp:45: error: undeclared identifier 'DXInit'`
- [ ] `src/shader.cpp:112: warning: implicit conversion loses integer precision`
- [ ] `src/main.cpp:72: error: expected ';' after return statement`

---

## Notes

- The Build Agent operates in continuous integration but can be triggered locally through scripts or configured bots.
- The agent’s output may be displayed in a special markdown file, comment, or notification in the chat, depending on implementation.