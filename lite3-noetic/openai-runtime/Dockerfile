FROM python:3.12-slim

RUN python -m pip install --no-cache-dir 'openai==3.6.0'

COPY openai-runtime/server.py /app/server.py

USER nobody
EXPOSE 8765
ENTRYPOINT ["python", "/app/server.py"]
