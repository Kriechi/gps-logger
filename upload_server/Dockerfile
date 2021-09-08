FROM python:3.9

ENV PYTHONUNBUFFERED=true

RUN mkdir -p /app /app/config
WORKDIR /app

COPY requirements.txt .
RUN python3 -m pip install -r requirements.txt

COPY upload_server.py .

ENTRYPOINT [ "./upload_server.py" ]
