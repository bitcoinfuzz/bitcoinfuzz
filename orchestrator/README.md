# Orchestrator Scaffold

This package is the first code anchor for the bitcoinfuzz infrastructure project.

## Current scope

- Load project configuration from the repository root.
- Read target metadata from `docker-compose.yml`.
- Expose a basic FastAPI app with health and target listing endpoints.

## Next step

Add campaign state, Docker lifecycle management, and persistence once the target registry parsing is validated.

## Quickstart

Install dependencies and run the app locally (recommended in a virtualenv):

```bash
pip install -r ../requirements.txt
./run_orchestrator.sh
```