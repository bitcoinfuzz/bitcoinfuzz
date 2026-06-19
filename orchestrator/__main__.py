"""CLI entrypoint for the orchestrator package.

Run with: `python -m orchestrator` to start a development server.
"""
import uvicorn

from .app import app


def main() -> None:
    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="info")


if __name__ == "__main__":
    main()
