# Agents - Behaviors and Specifications

## Overview
This directory contains agent configurations and documentation for the render test project.

## Agent Behaviors

### Default Agent
- **Purpose**: Primary agent for rendering pipeline tasks
- **Capabilities**: 
  - Process render requests
  - Manage resource allocation
  - Handle error reporting
  - Monitor performance metrics

### Documentation Agent
- **File**: `documentation-agent.md`
- **Purpose**: Capture all newly implemented render engine features in markdown docs under `AI-Documented/`
- **Capabilities**:
  - Detect new/updated render features
  - Create/update per-feature markdown files
  - Maintain `AI-Documented/README.md` feature index
  - Record controls, constraints, and implementation notes

### Docsite Agent
- **File**: `docsite-agent.md`
- **Purpose**: Build and maintain a high-quality MkDocs documentation site from markdown sources
- **Capabilities**:
  - Configure `mkdocs.yml` theme/nav/plugins
  - Organize docs information architecture by category
  - Improve docs-site UX with HTML/CSS-safe customizations
  - Validate local build/serve workflows and link integrity

## Specifications

### Configuration Requirements
- Agent must support concurrent task processing
- Maximum timeout: 30 seconds per task
- Memory limit: 512MB per agent instance
- Requires valid authentication token

### Communication Protocol
- JSON-based message format
- REST API endpoints
- Async/await support
- Error response handling

## Setup Instructions

1. Configure agent environment variables
2. Initialize agent service
3. Register with main render pipeline
4. Validate connectivity

## Monitoring

- Health checks every 5 minutes
- Log aggregation enabled
- Performance metrics tracked
- Alert thresholds configured

---
For detailed implementation, see individual agent configuration files.