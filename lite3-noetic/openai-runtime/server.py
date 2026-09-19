#!/usr/bin/env python3
"""Loopback-only streaming bridge from ROS Noetic to the official OpenAI SDK."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import sys


MAX_REQUEST_BYTES = 128 * 1024


def log_request_error(exc):
    """Log bounded diagnostics without ever printing the API key."""
    message = "%s: %s" % (type(exc).__name__, exc)
    api_key = os.environ.get("OPENAI_API_KEY", "")
    if api_key:
        message = message.replace(api_key, "[redacted]")
    print("OpenAI bridge request failed: " + message[:1000], file=sys.stderr, flush=True)


def build_instructions(emotion):
    """Build a per-turn voice policy without treating mood as stale context."""
    return (
        "You are the conversational voice of a simulated quadruped robot. "
        "Respond naturally in one to three sentences and no more than 100 words, "
        "even when the user requests a detailed answer or list. Prioritize the "
        "emotional meaning and intensity of the user's latest message over the "
        "mood of earlier turns. Respond warmly to joy, gently to sadness or fear, "
        "and to direct hostility with a calm, specific acknowledgement and a "
        "clear, non-combative boundary; do not stay cheerful, use a canned "
        "'I'm here to listen' reply, mirror abuse, or claim personal distress. "
        "The robot's current internal emotional state is %s; let it influence "
        "word choice only after addressing the latest message. Never claim to "
        "control hardware." % emotion
    )


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, _format, *_args):
        return

    def _send_json(self, status, payload):
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            self._send_json(200, {
                "ok": True,
                "sdk": "official",
                "api_key_present": bool(os.environ.get("OPENAI_API_KEY")),
                "test_mode": os.environ.get("OPENAI_BRIDGE_TEST_MODE") == "1",
            })
        else:
            self._send_json(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/v1/stream":
            self._send_json(404, {"error": "not found"})
            return
        streaming_started = False
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length <= 0 or length > MAX_REQUEST_BYTES:
            self._send_json(413, {"error": "invalid request size"})
            return
        try:
            request = json.loads(self.rfile.read(length).decode("utf-8"))
            text = str(request["text"]).strip()
            history = request.get("history", [])
            emotion = str(request.get("emotion", "neutral"))
            model = str(request.get("model", "gpt-5-mini"))
            timeout = max(1.0, min(60.0, float(request.get("timeout", 20.0))))
            max_tokens = max(16, min(512, int(request.get("max_output_tokens", 180))))
            if not text or not isinstance(history, list):
                raise ValueError("invalid input")
        except (KeyError, TypeError, ValueError, UnicodeError):
            self._send_json(400, {"error": "invalid request"})
            return
        try:
            if os.environ.get("OPENAI_BRIDGE_TEST_MODE") == "1":
                self.send_response(200)
                self.send_header("Content-Type", "application/x-ndjson")
                self.send_header("Connection", "close")
                self.end_headers()
                streaming_started = True
                self.wfile.write(b'{"type":"delta","text":"offline bridge"}\n')
                self.wfile.write(b'{"type":"delta","text":" test"}\n')
                self.wfile.write(b'{"type":"done"}\n')
                self.wfile.flush()
                return
            from openai import OpenAI

            client = OpenAI(timeout=timeout, max_retries=0)
            inputs = list(history[-12:])
            inputs.append({"role": "user", "content": text})
            upstream = client.responses.create(
                model=model,
                instructions=build_instructions(emotion),
                input=inputs,
                max_output_tokens=max_tokens,
                reasoning={"effort": "minimal"},
                text={"verbosity": "low"},
                store=False,
                stream=True,
                timeout=timeout,
            )
            self.send_response(200)
            self.send_header("Content-Type", "application/x-ndjson")
            self.send_header("Connection", "close")
            self.end_headers()
            streaming_started = True
            try:
                for event in upstream:
                    event_type = getattr(event, "type", "")
                    if event_type == "response.output_text.delta":
                        delta = getattr(event, "delta", "")
                        if delta:
                            self.wfile.write((json.dumps({"type": "delta", "text": delta}) + "\n").encode("utf-8"))
                            self.wfile.flush()
                    elif event_type in ("response.failed", "response.incomplete"):
                        raise RuntimeError("response did not complete")
                self.wfile.write(b'{"type":"done"}\n')
                self.wfile.flush()
            finally:
                close = getattr(upstream, "close", None)
                if close is not None:
                    close()
        except (BrokenPipeError, ConnectionResetError):
            return
        except Exception as exc:
            log_request_error(exc)
            try:
                if streaming_started:
                    self.wfile.write(b'{"type":"error","message":"request failed"}\n')
                    self.wfile.flush()
                else:
                    self._send_json(503, {"error": "request failed"})
            except (BrokenPipeError, ConnectionResetError):
                pass
        finally:
            self.close_connection = True


def main():
    port = int(os.environ.get("OPENAI_BRIDGE_PORT", "8765"))
    if not 1 <= port <= 65535:
        raise ValueError("OPENAI_BRIDGE_PORT must be in [1, 65535]")
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    server.daemon_threads = True
    server.serve_forever(poll_interval=0.25)


if __name__ == "__main__":
    main()
