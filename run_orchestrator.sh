#!/usr/bin/env bash
set -euo pipefail
PYTHONPATH="${PYTHONPATH:-}:$(dirname "$0")"
uvicorn orchestrator.app:app --host 127.0.0.1 --port 8000 --reload
